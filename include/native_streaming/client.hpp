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
#include <native_streaming/session.hpp>
#include <native_streaming/authentication.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

/// @brief Connects to specified server address, creates and returns new Session via callback when connection established
class Client : public std::enable_shared_from_this<Client>
{
public:
    explicit Client(const std::string& host,
                    const std::string& port,
                    const std::string& path,
                    const Authentication& authentication,
                    OnNewSessionCallback onNewSessionCallback,
                    OnCompleteCallback onResolveFailCallback,
                    OnCompleteCallback onConnectFailCallback,
                    OnCompleteCallback onHandshakeFailCallback,
                    std::shared_ptr<boost::asio::io_context> ioContextPtr,
                    LogCallback logCallback);
    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    /// @brief initiates asynchronous connection to remote server. Connection procedure contains next steps:
    /// 1. resolve address specified in Client constructor
    /// 2. connect to server using endpoind resolved from address
    /// 3. do web-socket handshake
    void connect();

private:
    /// @brief callback called when resolving of remote server address is finished
    /// @param ec error_code object indicates resolving failure
    /// @param results represents connection endpoint resolved from address
    void onResolve(const boost::beast::error_code& ec, boost::asio::ip::tcp::resolver::results_type results);

    /// @brief callback called when connection to remote server is finished
    /// @param ec error_code object indicates connection failure
    /// @param wsStream websocket stream object associated with connection
    void onConnect(const boost::system::error_code& ec, std::shared_ptr<WebsocketStream> wsStream);

    /// @brief callback called when web-socket handshake finished for new connection
    /// @param ec error_code object indicates handshake failure
    /// @param wsStream websocket stream object associated with connection
    void onUpgradeConnection(const boost::system::error_code& ec, std::shared_ptr<WebsocketStream> wsStream);

    /// @brief creates a connection Session using provided web-socket stream object
    /// @param wsStream web-socket stream object which provides as a R/W interface for connection
    /// @return pointer to created Session object
    std::shared_ptr<Session> createSession(std::shared_ptr<WebsocketStream> wsStream);

    /// async operations handler
    std::shared_ptr<boost::asio::io_context> ioContextPtr;

    /// Redirects log calls
    LogCallback logCallback;

    /// @brief IP address of remote server client connects to
    std::string host;

    /// @brief string representation of server port number with web-socket service running on it
    std::string port;

    /// @brief additional path to web-socket service on a server. Usually is "/"
    std::string path;

    /// @brief The TCP resolver
    boost::asio::ip::tcp::resolver resolver;

    /// @brief callback used to provide newly created Session to outside world, e.g. to protocol handler
    OnNewSessionCallback onNewSessionCallback;

    /// @brief callback to be called if resolving address of remote server is failed
    OnCompleteCallback onResolveFailCallback;

    /// @brief callback to be called if connection to remote server is failed
    OnCompleteCallback onConnectFailCallback;

    /// @brief callback to be called if handshake with a remote server is failed
    OnCompleteCallback onHandshakeFailCallback;

    /// @brief a structure holding authentication information
    Authentication authentication;
};

END_NAMESPACE_NATIVE_STREAMING
