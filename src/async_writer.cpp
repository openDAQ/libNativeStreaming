#include <native_streaming/async_writer.hpp>
#include <boost/asio/read.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

using namespace boost::asio::detail;

AsyncWriter::AsyncWriter(boost::asio::io_context& ioContextRef, std::shared_ptr<WebsocketStream> wsStream, LogCallback logCallback)
    : wsStream(wsStream)
    , logCallback(logCallback)
    , ioContextRef(ioContextRef)
    , strand(ioContextRef)
{
}

void AsyncWriter::scheduleWrite(std::vector<WriteTask>&& tasks)
{
    ioContextRef.post(strand.wrap(
        [this, tasks = std::move(tasks), shared_self = shared_from_this()]() mutable
        {
            queueWriteTasks(std::move(tasks));
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

void AsyncWriter::queueWriteTasks(std::vector<WriteTask>&& tasks)
{
    bool writing = !writeTasksQueue.empty();
    writeTasksQueue.push(std::move(tasks));
    if (!writing)
    {
        doWrite(writeTasksQueue.front());
    }
}

void AsyncWriter::doWrite(const std::vector<WriteTask>& tasks)
{
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
    const auto& tasks = writeTasksQueue.front();
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
