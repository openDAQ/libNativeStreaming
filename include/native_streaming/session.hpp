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
#include <native_streaming/async_reader.hpp>
#include <native_streaming/async_writer.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

class Session;

using OnNewSessionCallback = std::function<void(std::shared_ptr<Session>)>;
using OnSessionErrorCallback = std::function<void(const std::string&, std::shared_ptr<Session>)>;

/// @brief handles sending / receiving data thru web-socket connection.
class Session : public std::enable_shared_from_this<Session>
{
public:
    explicit Session(std::shared_ptr<boost::asio::io_context> ioContextPtr,
                     std::shared_ptr<WebsocketStream> wsStream,
                     std::shared_ptr<void> userContext,
                     boost::beast::role_type role,
                     LogCallback logCallback);
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    /// @brief closes a web-socket stream object disabling R/W operations.
    /// Can be called before Session object destruction.
    /// @param onClosedCallback callback function object to be called on close completion
    void close(OnCompleteCallback onClosedCallback = [](const boost::system::error_code&){});

    /// @brief schedules write tasks
    /// @param tasks write tasks to execute
    void scheduleWrite(const std::vector<WriteTask>& tasks);

    /// @brief shedule read sequence specified by entry read task
    /// @param entryTask first read task in a read sequence
    void scheduleRead(const ReadTask& entryTask);

    /// @brief sets a callbacks to be called when read/write operation is failed
    /// @param onWriteErrorCallback write operation failed callback
    /// @param onReadErrorCallback read operation failed callback
    void setErrorHandlers(OnSessionErrorCallback onWriteErrorCallback, OnSessionErrorCallback onReadErrorCallback);

    /// @brief starts connection activity monitoring and sending websocket pongs periodically
    /// @param connectionAliveCallback callback to be called on succedeed read/write and received websocket pongs
    /// @param heartbeatPeriod interval of sending the websocket pongs
    void startConnectionActivityMonitoring(OnConnectionAliveCallback connectionAliveCallback,
                                           std::chrono::milliseconds heartbeatPeriod);

    /// @brief returns true if websocket stream related to session is open, false otherwise
    bool isOpen();

    /// @brief returns user context object, usualy a pointer to the authenticated user object
    std::shared_ptr<void> getUserContext();

private:
    /// @brief applies additional settings to web-socket stream
    void setOptions();

    /// @brief schedules an async websocket pong each time heartbeat timer expired
    void schedulePong();

    /// @brief restarts heartbeat timer
    void restartHeartbeatTimer();

    /// @brief determines if Session belongs to server or client
    boost::beast::role_type role;

    /// @brief Redirects log calls
    LogCallback logCallback;

    /// @brief Callback to trigger upon successful I/O operation or upon receiving a WebSocket pong,
    /// confirming an active state of connection
    OnConnectionAliveCallback connectionAliveCallback = []() {};

    /// @brief async operations handler
    std::shared_ptr<boost::asio::io_context> ioContextPtr;

    /// @brief handles receiving data
    std::shared_ptr<AsyncReader> reader;

    /// @brief handles sending data
    std::shared_ptr<AsyncWriter> writer;

    /// @brief web-socket stream object which provides as a R/W interface for connection
    std::shared_ptr<WebsocketStream> wsStream;

    /// @brief user context, usualy a pointer to the authenticated user object
    std::shared_ptr<void> userContext;

    /// @brief timer used to send websocket pongs
    std::shared_ptr<boost::asio::steady_timer> heartbeatTimer;

    /// @brief interval of sending the websocket pongs
    std::chrono::milliseconds heartbeatPeriod;
};

END_NAMESPACE_NATIVE_STREAMING
