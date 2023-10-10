/*
 * Copyright 2022-2023 Blueberry d.o.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <native_streaming/common.hpp>
#include <boost/asio/streambuf.hpp>
#include <native_streaming/logging.hpp>
#include <boost/asio/io_context_strand.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

class ReadTask;

/// @brief type of callback to be called on each read task completion
/// @param data pointer to read data
/// @param size count of read bytes
/// @return object which represents read task to be executed next
/// The task object with nullptr handler is used to specify the end of read sequence
using ReadHandler = std::function<ReadTask(const void* data, size_t size)>;

/// @brief represents read task to be executed by AsyncReader
class ReadTask
{
public:
    /// @brief default constructor, constructed object should be used to finish task sequence
    ReadTask()
        : handler(nullptr)
        , bytesToRead(0)
    {
    }

    explicit ReadTask(ReadHandler handler, size_t bytesToRead)
        : handler(handler)
        , bytesToRead(bytesToRead)
    {
        assert(handler != nullptr);
        assert(bytesToRead != 0);
    }

    ReadHandler getHandler() const
    {
        return handler;
    }

    size_t getSize() const
    {
        return bytesToRead;
    }

private:
    /// @brief a callback to be called after requested amount of bytes is read
    ReadHandler handler;

    /// @brief amount of bytes to read within the task
    size_t bytesToRead;
};

/// @brief handles receiving data thru web-socket connection.
class AsyncReader : public std::enable_shared_from_this<AsyncReader>
{
public:
    explicit AsyncReader(boost::asio::io_context& ioContextRef, std::shared_ptr<WebsocketStream> wsStream, LogCallback logCallback);

    AsyncReader(const AsyncReader&) = delete;
    AsyncReader& operator=(const AsyncReader&) = delete;

    /// @brief shedule read sequence specified by entry read task containing:
    /// count of bytes to read and callback called on read completion - callback returns next
    /// read task to execute.
    /// @param entryTask first read task in a read sequence
    void scheduleRead(const ReadTask& entryTask);

    /// @brief sets a callback to be called when read operation fails
    /// @param onErrorCallback callback
    void setErrorHandler(OnCompleteCallback onErrorCallback);

private:
    /// @brief asynchronously reads specified count of bytes and calls specified callback on read completion
    /// @param bytesToRead count of bytes to read
    /// @param onReadCallback callback to be called on read completion
    void read(std::size_t bytesToRead, OnRWCallback onReadCallback);

    /// @brief wraps read operation call
    /// @param bytesToRead count of bytes to read
    void doRead(std::size_t bytesToRead);

    /// @brief returns a raw pointer to read buffer
    const void* data() const;

    /// @brief consumes (removes) size bytes from read buffer
    /// @param size count of bytes to consume
    void consume(size_t size);

    /// @brief returns number of bytes in read buffer
    size_t bufferSize();

    /// @brief web-socket stream object which provides a read interface
    std::shared_ptr<WebsocketStream> wsStream;

    /// @brief Redirects log calls
    LogCallback logCallback;

    /// @brief read buffer for temporary storing data received thru a connection
    boost::asio::streambuf buffer;

    /// @brief reference to async operations handler
    boost::asio::io_context& ioContextRef;

    /// @brief strand to wrap async read operations
    boost::asio::io_context::strand strand;

    /// @brief current handling read task
    ReadTask pendingTask;

    /// @brief called on each read operation completion
    /// @param ec error_codecode object indicates read failure
    /// @param size count of bytes read during operation
    void readDone(const boost::system::error_code& ec, std::size_t size);

    /// @brief callback to be called when read operation fails
    OnCompleteCallback errorHandler;
};

END_NAMESPACE_NATIVE_STREAMING
