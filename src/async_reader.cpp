#include <native_streaming/async_reader.hpp>
#include <boost/asio/read.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

AsyncReader::AsyncReader(boost::asio::io_context& ioContextRef, std::shared_ptr<WebsocketStream> wsStream, LogCallback logCallback)
    : wsStream(wsStream)
    , logCallback(logCallback)
    , ioContextRef(ioContextRef)
    , strand(ioContextRef)
    , pendingTask()
{
}

void AsyncReader::read(std::size_t bytesToRead, OnRWCallback onReadCallback)
{
    size_t bytesReady = bufferSize();
    if (bytesReady >= bytesToRead)
    {
        onReadCallback(boost::system::error_code(), bytesToRead);
    }
    else
    {
        bytesToRead = bytesToRead - bytesReady;
        boost::asio::async_read(*wsStream, buffer, boost::asio::transfer_at_least(bytesToRead), onReadCallback);
    }
}

void AsyncReader::scheduleRead(const ReadTask& entryTask)
{
    assert(entryTask.getHandler() != nullptr);
    assert(entryTask.getSize() != 0);
    pendingTask = entryTask;
    ioContextRef.post(strand.wrap(
        [this, shared_self = shared_from_this()]()
        {
            doRead(pendingTask.getSize());
        }));
}

void AsyncReader::setErrorHandler(OnCompleteCallback onErrorCallback)
{
    errorHandler = onErrorCallback;
}

void AsyncReader::setConnectionAliveHandler(OnConnectionAliveCallback connectionAliveCallback)
{
    this->connectionAliveCallback = connectionAliveCallback;
}

void AsyncReader::readDone(const boost::system::error_code& ec, std::size_t /*size*/)
{
    if (!ec)
    {
        this->connectionAliveCallback();

        auto readHandler = pendingTask.getHandler();
        auto readSize = pendingTask.getSize();

        pendingTask = readHandler(data(), readSize);
        consume(readSize);

        if (pendingTask.getHandler() != nullptr)
        {
            doRead(pendingTask.getSize());
        }
        else
        {
            NS_LOG_T("Read sequence finished");
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
            NS_LOG_E("Reading failed {}", ec.message());
        }
    }
}

void AsyncReader::doRead(std::size_t bytesToRead)
{
    read(bytesToRead,
         [this, shared_self = shared_from_this()](const boost::system::error_code& ec, std::size_t size)
         {
             readDone(ec, size);
         });
}

const void* AsyncReader::data() const
{
    return boost::asio::buffer_cast<const void*>(buffer.data());
}

void AsyncReader::consume(size_t size)
{
    buffer.consume(size);
}

size_t AsyncReader::bufferSize()
{
    return buffer.size();
}

END_NAMESPACE_NATIVE_STREAMING
