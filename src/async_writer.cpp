#include <native_streaming/async_writer.hpp>
#include <boost/asio/read.hpp>
#include <boost/container/small_vector.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

using namespace boost::asio::detail;

AsyncWriter::AsyncWriter(boost::asio::io_context& ioContextRef, std::shared_ptr<WebsocketStream> wsStream, LogCallback logCallback)
    : wsStream(wsStream)
    , logCallback(logCallback)
    , ioContextRef(ioContextRef)
    , strand(ioContextRef)
    , timeoutReached(false)
{
}

void AsyncWriter::scheduleWrite(BatchedWriteTasks&& tasks, OptionalWriteDeadline&& deadlineTime)
{
    ioContextRef.post(strand.wrap(
        [this, tasks = std::move(tasks), deadlineTime = std::move(deadlineTime), shared_self = shared_from_this()]() mutable
        {
            queueBatchWrite(std::move(tasks), std::move(deadlineTime));
        }));
}

void AsyncWriter::setErrorHandler(OnCompleteCallback onErrorCallback)
{
    errorHandler = onErrorCallback;
}

void AsyncWriter::setConnectionAliveHandler(OnConnectionAliveCallback connectionAliveCallback)
{
    this->connectionAliveCallback = connectionAliveCallback;
}

void AsyncWriter::setWriteTimedOutHandler(OnWriteTaskTimedOutCallback writeTaskTimeoutHandler)
{
    this->writeTaskTimeoutHandler = writeTaskTimeoutHandler;
}

void AsyncWriter::queueBatchWrite(BatchedWriteTasks&& tasks, OptionalWriteDeadline&& deadlineTime)
{
    if (timeoutReached)
        return;

    OptionalDeadlineTimer deadlineTimer = nullptr;
    if (deadlineTime.has_value())
    {
        auto now = std::chrono::steady_clock::now();
        auto deadlineValue = deadlineTime.value();
        if (now > deadlineValue)
        {
            onTimeoutReached();
            return;
        }
        deadlineTimer = setupDeadlineTimer(deadlineValue);
    }

    bool writing = !writeTasksQueue.empty();
    writeTasksQueue.push(std::pair(std::move(tasks), std::move(deadlineTimer)));
    if (!writing)
    {
        doWrite(writeTasksQueue.front());
    }
}

void AsyncWriter::doWrite(const BatchedWriteTasksWithDeadline& tasksWithDeadline)
{
    if (timeoutReached)
        return;

    const auto& tasks = tasksWithDeadline.first;

    boost::container::small_vector<boost::asio::const_buffer, 16> buffers;

    for (const auto& task : tasks)
    {
        buffers.push_back(task.getBuffer());
    }

    wsStream->async_write(buffers,
                          strand.wrap(
                              [this, shared_self = shared_from_this()](const boost::system::error_code& ec, std::size_t size)
                              {
                                  writeDone(ec, size);
                              }));
}

void AsyncWriter::writeDone(const boost::system::error_code& ec, [[maybe_unused]] std::size_t size)
{
    const auto& tasks = writeTasksQueue.front().first;
    const auto tasksSize = tasks.size();
    for (const auto& task : tasks)
    {
        auto handler = task.getHandler();
        handler();
    }

    const auto& deadlineTimer = writeTasksQueue.front().second;
    if (deadlineTimer)
        deadlineTimer->cancel();

    writeTasksQueue.pop();
    if (!ec)
    {
        this->connectionAliveCallback();

        NS_LOG_T("Write done - tasks count: {}, bytes written: {}", tasksSize, size)
        if (!writeTasksQueue.empty())
        {
            doWrite(writeTasksQueue.front());
        }
    }
    else
    {
        if (errorHandler)
        {
            errorHandler(ec);
        }
        else
        {
            NS_LOG_E("Writing failed {}", ec.message());
        }
    }
}

void AsyncWriter::onTimeoutReached()
{
    if (timeoutReached)
        return;

    timeoutReached = true;
    writeTaskTimeoutHandler();
}

AsyncWriter::OptionalDeadlineTimer AsyncWriter::setupDeadlineTimer(const std::chrono::steady_clock::time_point& deadline)
{
    auto deadlineTimer = std::make_unique<boost::asio::steady_timer>(ioContextRef);
    deadlineTimer->expires_at(deadline);
    deadlineTimer->async_wait(
        [this, weak_self = weak_from_this()](const boost::system::error_code& ec)
        {
            if (ec)
                return;

            if (auto shared_self = weak_self.lock())
            {
                this->onTimeoutReached();
            }
        }
    );
    return deadlineTimer;
}

END_NAMESPACE_NATIVE_STREAMING
