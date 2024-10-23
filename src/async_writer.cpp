#include <native_streaming/async_writer.hpp>
#include <boost/asio/read.hpp>
#include <fmt/chrono.h>

BEGIN_NAMESPACE_NATIVE_STREAMING

using namespace boost::asio::detail;

AsyncWriter::AsyncWriter(boost::asio::io_context& ioContextRef, std::shared_ptr<WebsocketStream> wsStream, LogCallback logCallback)
    : wsStream(wsStream)
    , logCallback(logCallback)
    , ioContextRef(ioContextRef)
    , strand(ioContextRef)
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
    bool writing = !writeTasksQueue.empty();
    writeTasksQueue.push(std::pair(std::move(tasks), std::move(deadlineTime)));
    if (!writing)
    {
        doWrite(writeTasksQueue.front());
    }
}

void AsyncWriter::doWrite(const BatchedWriteTasksWithDeadline& tasksWithDeadline)
{
    const auto& tasks = tasksWithDeadline.first;
    const auto& deadlineTimepoint = tasksWithDeadline.second;

    if (deadlineTimepoint.has_value())
    {
        auto now = std::chrono::system_clock::now();
        auto deadlineValue = deadlineTimepoint.value();
        if (now > deadlineValue)
        {
            NS_LOG_E("Write task canceled due to timeout expiration: time-now {:%Y-%m-%d %H:%M:%S}, deadline-time {:%Y-%m-%d %H:%M:%S}",
                     now,
                     deadlineValue);
            writeTaskTimeoutHandler();
            return;
        }
    }

    std::vector<boost::asio::const_buffer> buffers;
    buffers.reserve(tasks.size());

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

void AsyncWriter::writeDone(const boost::system::error_code& ec, std::size_t size)
{
    const auto& tasks = writeTasksQueue.front().first;
    const auto tasksSize = tasks.size();
    for (const auto& task : tasks)
    {
        auto handler = task.getHandler();
        handler();
    }

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

END_NAMESPACE_NATIVE_STREAMING
