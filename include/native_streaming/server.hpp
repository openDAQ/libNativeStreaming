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

using OnAuthenticateCallback = std::function<bool(const Authentication& authentication)>;

/// @brief accepts incoming connections on specified port, creates and returns new Session object via
/// callback for each connected client
class Server : public std::enable_shared_from_this<Server>
{
public:
    explicit Server(OnNewSessionCallback onNewSessionCallback,
                    OnAuthenticateCallback onAuthenticateCallback,
                    std::shared_ptr<boost::asio::io_context> ioContextPtr,
                    LogCallback logCallback);
    ~Server();
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    /// @brief sets up Tcp acceptors on specified port and starts listening for incoming connections in
    /// asynchronous manner, starts async operation handling
    /// @param port Tcp port used for incoming connections
    /// @return default (success) error code object if all connection steps succeeded, error otherwise
    boost::system::error_code start(uint16_t port);

    /// @brief stops listening for incoming connections, stops async operation handling
    void stop();

private:
    /// @brief callback called when new Tcp connection acception finished by server
    /// @param tcpAcceptor Tcp acceptor which accepts connection
    /// @param ec error_code object indicates connection acception failed
    /// @param socket tcp socket associated with new connection
    void onAcceptTcpConnection(boost::asio::ip::tcp::acceptor& tcpAcceptor,
                               const boost::system::error_code& ec,
                               boost::asio::ip::tcp::socket&& socket);

    /// @brief callback called when connect request headers have been read by the server
    /// @param ec error_code object indicates if headers were read successfuly
    /// @param wsStream web-socket stream object which provides as a R/W interface for connection
    /// @param request object which holds connect request parameters
    void onReadAcceptRequest(const boost::system::error_code& ec,
                             const std::shared_ptr<WebsocketStream>& wsStream,
                             boost::beast::http::request<boost::beast::http::string_body>& request);

    /// @brief callback called when web-socket handshake finished for new connection
    /// @param ec error_code object indicates handshake failure
    /// @param wsStream websocket stream object associated with connection
    void onUpgradeConnection(const boost::system::error_code& ec, std::shared_ptr<WebsocketStream> wsStream);

    /// @brief starts accepting incoming Tcp asynchronously with specified acceptor
    /// @param tcpAcceptor Tcp connection acceptor
    void startTcpAccept(boost::asio::ip::tcp::acceptor& tcpAcceptor);

    /// @brief stops asynchronous accepting connections with specified acceptor
    /// @param tcpAcceptor Tcp connection acceptor
    void stopTcpAccept(boost::asio::ip::tcp::acceptor& tcpAcceptor);

    /// @brief creates a connection Session using provided web-socket stream object
    /// @param wsStream web-socket stream object which provides as a R/W interface for connection
    /// @return pointer to created Session object
    std::shared_ptr<Session> createSession(std::shared_ptr<WebsocketStream> wsStream);

    /// async operations handler
    std::shared_ptr<boost::asio::io_context> ioContextPtr;

    /// Redirects log calls
    LogCallback logCallback;

    /// @brief callback used to provide newly created Session to outside world, e.g. to protocol handler
    OnNewSessionCallback onNewSessionCallback;

    /// @brief callback which is triggered on authenticaiton step. It should return true for successful authentication and false otherwise.
    OnAuthenticateCallback onAuthenticateCallback;

    /// Tcp connection acceptor binded to IPv4
    boost::asio::ip::tcp::acceptor tcpAcceptorV4;

    /// Tcp connection acceptor binded to IPv6
    boost::asio::ip::tcp::acceptor tcpAcceptorV6;

    /// Buffer for reading request headers during accept step
    boost::asio::streambuf acceptBuffer;

    /// Object for holding request parameters druging accept step
    boost::beast::http::request<boost::beast::http::string_body> acceptRequest;
};

END_NAMESPACE_NATIVE_STREAMING
