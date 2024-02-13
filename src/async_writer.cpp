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

void AsyncWriter::scheduleWrite(const std::vector<WriteTask>& tasks)
{
    ioContextRef.post(strand.wrap(
        [this, tasks, shared_self = shared_from_this()]()
        {
            queueWriteTasks(tasks);
        }));
}

void AsyncWriter::setErrorHandler(OnCompleteCallback onErrorCallback)
{
    errorHandler = onErrorCallback;
}

void AsyncWriter::queueWriteTasks(const std::vector<WriteTask>& tasks)
{
    bool writing = !writeTasksQueue.empty();
    writeTasksQueue.push(tasks);
    if (!writing)
    {
        doWrite(writeTasksQueue.front());
    }
}

void AsyncWriter::doWrite(const std::vector<WriteTask>& tasks)
{
    std::vector<boost::asio::const_buffer> buffers;
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
    auto tasks = writeTasksQueue.front();
    for (const auto& task : tasks)
    {
        auto handler = task.getHandler();
        handler();
    }

    writeTasksQueue.pop();
    if (!ec)
    {
        NS_LOG_T("Write done - tasks count: {}, bytes written: {}", tasks.size(), size);
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
