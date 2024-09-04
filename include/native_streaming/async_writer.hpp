/*
 * Copyright 2022-2024 openDAQ d.o.o.
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
#include <native_streaming/logging.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <queue>

BEGIN_NAMESPACE_NATIVE_STREAMING

using WriteHandler = std::function<void()>;

/// @brief represents a write task to be executed by AsyncWriter
class WriteTask
{
public:
    explicit WriteTask(boost::asio::const_buffer buffer, WriteHandler handler)
        : buffer(buffer)
        , handler(handler)
    {
        assert(handler != nullptr);
        assert(buffer.data() != nullptr);
        assert(buffer.size() != 0);
    }

    boost::asio::const_buffer getBuffer() const
    {
        return buffer;
    }

    WriteHandler getHandler() const
    {
        return handler;
    }

private:
    /// @brief buffer to be written
    boost::asio::const_buffer buffer;

    /// @brief a callback to be called after data is written, used to release memory
    WriteHandler handler;
};

/// @brief handles sending data thru web-socket connection.
class AsyncWriter : public std::enable_shared_from_this<AsyncWriter>
{
public:
    explicit AsyncWriter(boost::asio::io_context& ioContextRef, std::shared_ptr<WebsocketStream> wsStream, LogCallback logCallback);

    AsyncWriter(const AsyncWriter&) = delete;
    AsyncWriter& operator=(const AsyncWriter&) = delete;

    /// @brief schedules write tasks
    /// @param tasks write tasks to execute
    void scheduleWrite(std::vector<WriteTask>&& tasks);

    /// @brief sets a callback to be called when write operation is failed
    /// @param onErrorCallback callback
    void setErrorHandler(OnCompleteCallback onErrorCallback);

    /// @brief sets a callback to be called on each completed write operation
    /// @param connectionAliveCallback callback
    void setConnectionAliveHandler(OnConnectionAliveCallback connectionAliveCallback);

private:
    /// @brief pushes tasks into queue
    /// @param tasks write tasks to queue
    void queueWriteTasks(std::vector<WriteTask>&& tasks);

    /// @brief wraps write operation call
    void doWrite(const std::vector<WriteTask>& tasks);

    /// @brief called on each write operation completion
    /// @param ec error_codecode object indicates write failure
    /// @param size count of bytes written during operation
    void writeDone(const boost::system::error_code& ec, std::size_t);

    /// @brief web-socket stream object which provides as a write interface for connection
    std::shared_ptr<WebsocketStream> wsStream;

    /// @brief Redirects log calls
    LogCallback logCallback;

    /// @brief reference to async operations handler
    boost::asio::io_context& ioContextRef;

    /// @brief strand to wrap async write operations
    boost::asio::io_context::strand strand;

    /// @brief queue for write tasks groups
    std::queue<std::vector<WriteTask>> writeTasksQueue;

    /// @brief callback to be called when write operation is failed
    OnCompleteCallback errorHandler;

    /// @brief Callback to be called on each completed write operation,
    /// confirming an active state of connection
    OnConnectionAliveCallback connectionAliveCallback = []() {};
};

END_NAMESPACE_NATIVE_STREAMING
