/*
 * Copyright 2022-2026 openDAQ d.o.o.
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

#if NATIVE_STREAMING_ENABLE_TLS
#include <native_streaming/tls.hpp>
#endif

BEGIN_NAMESPACE_NATIVE_STREAMING

using OnAuthenticateCallback = std::function<bool(const Authentication& authentication, std::shared_ptr<void>& userContextOut)>;

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

    /// @brief sets up Tcp acceptors on specified port and starts listening for incoming plaintext
    /// connections in asynchronous manner, starts async operation handling
    /// @param port Tcp port used for incoming connections
    /// @return default (success) error code object if all connection steps succeeded, error otherwise
    boost::system::error_code start(uint16_t port);

#if NATIVE_STREAMING_ENABLE_TLS

    /// @brief sets up Tcp acceptors on specified port and starts listening for incoming
    /// TLS-encrypted connections. Can be used alongside start(), in which case the server accepts
    /// plaintext connections on one port and encrypted ones on the other
    /// @param port Tcp port used for incoming connections, distinct from the plaintext one
    /// @param certFile path to the server certificate chain (PEM)
    /// @param keyFile path to the server private key (PEM)
    /// @param caFile path to a PEM file of trusted CA certificates used to verify client
    /// certificates. If non-empty, mutual TLS is enabled: clients must present a certificate signed
    /// by one of these authorities. If empty, client certificates are not requested
    /// @return default (success) error code object if all steps succeeded, error otherwise -
    /// including a missing or malformed secret, which is reported rather than thrown
    boost::system::error_code startTls(uint16_t port,
                                       const std::string& certFile,
                                       const std::string& keyFile,
                                       const std::string& caFile = {});

#endif

    /// @brief stops listening for incoming connections, stops async operation handling
    void stop();

protected:
    /// a port the server listens on: the pair of acceptors bound to it, and what the
    /// connections accepted through them are made of
    struct Listener
    {
        explicit Listener(boost::asio::io_context& ioContext)
            : tcpAcceptorV4(ioContext)
            , tcpAcceptorV6(ioContext)
        {
        }

        /// Tcp connection acceptor binded to IPv4
        boost::asio::ip::tcp::acceptor tcpAcceptorV4;

        /// Tcp connection acceptor binded to IPv6
        boost::asio::ip::tcp::acceptor tcpAcceptorV6;

#if NATIVE_STREAMING_ENABLE_TLS

        /// the SSL context the accepted streams are created with. Null for a listener
        /// accepting plaintext connections
        std::unique_ptr<boost::asio::ssl::context> sslContext;

#endif
    };

    /// @brief per-connection state of the accept step: the web-socket stream being set up along with
    /// the buffer and the request object used to read the connect request headers into.
    /// One object is created for each accepted Tcp connection and is kept alive by the completion
    /// handlers of the accept sequence until the web-socket handshake is done, so that concurrently
    /// accepted connections never share it.
    template <typename Stream>
    struct AcceptOp
    {
        explicit AcceptOp(std::shared_ptr<Stream> wsStream)
            : wsStream(std::move(wsStream))
        {
        }

        /// @brief web-socket stream object which provides as a R/W interface for connection
        std::shared_ptr<Stream> wsStream;

        /// @brief buffer for reading request headers during accept step
        boost::asio::streambuf buffer;

        /// @brief object for holding request parameters during accept step
        boost::beast::http::request<boost::beast::http::string_body> request;
    };

    /// @brief callback called when new Tcp connection acception finished by server
    /// @param listener the listener which accepted the connection
    /// @param tcpAcceptor Tcp acceptor which accepts connection, owned by the listener
    /// @param ec error_code object indicates connection acception failed
    /// @param socket tcp socket associated with new connection
    virtual void onAcceptTcpConnection(const std::shared_ptr<Listener>& listener,
                                       boost::asio::ip::tcp::acceptor& tcpAcceptor,
                                       const boost::system::error_code& ec,
                                       boost::asio::ip::tcp::socket&& socket);

    /// @brief callback called when connect request headers have been read by the server
    /// @param ec error_code object indicates if headers were read successfuly
    /// @param acceptOp per-connection state of the accept step, holds the web-socket stream and the
    /// request object the headers were read into
    template <typename Stream>
    void onReadAcceptRequest(const boost::system::error_code& ec, const std::shared_ptr<AcceptOp<Stream>>& acceptOp);

    /// @brief callback called when web-socket handshake finished for new connection
    /// @param ec error_code object indicates handshake failure
    /// @param wsStream websocket stream object associated with connection
    /// @param user context, usualy a pointer to the authenticated user object
    template <typename Stream>
    void onUpgradeConnection(const boost::system::error_code& ec,
                             std::shared_ptr<Stream> wsStream,
                             const std::shared_ptr<void>& userContext);

private:
    /// @brief binds the acceptors of a listener to the given port and starts accepting on them
    /// @param listener the listener to start
    /// @param port Tcp port used for incoming connections
    /// @return default (success) error code object if at least one acceptor was set up
    boost::system::error_code startListener(const std::shared_ptr<Listener>& listener, uint16_t port);

    /// @brief starts accepting incoming Tcp asynchronously with specified acceptor
    /// @param listener the listener the acceptor belongs to
    /// @param tcpAcceptor Tcp connection acceptor, owned by the listener
    void startTcpAccept(const std::shared_ptr<Listener>& listener, boost::asio::ip::tcp::acceptor& tcpAcceptor);

    /// @brief stops asynchronous accepting connections with specified acceptor
    /// @param tcpAcceptor Tcp connection acceptor
    void stopTcpAccept(boost::asio::ip::tcp::acceptor& tcpAcceptor);

#if NATIVE_STREAMING_ENABLE_TLS

    /// @brief starts the TLS handshake, which an encrypted connection performs before the connect
    /// request headers can be read
    /// @param listener the listener which accepted the connection
    /// @param tcpAcceptor Tcp acceptor which accepted the connection, owned by the listener
    /// @param acceptOp per-connection state of the accept step
    template <typename Stream>
    void startTlsHandshake(const std::shared_ptr<Listener>& listener,
                           boost::asio::ip::tcp::acceptor& tcpAcceptor,
                           std::shared_ptr<AcceptOp<Stream>> acceptOp);

#endif

    /// @brief starts reading the connect request headers, and accepts the next connection once they
    /// have been read
    /// @param listener the listener which accepted the connection
    /// @param tcpAcceptor Tcp acceptor which accepted the connection, owned by the listener
    /// @param acceptOp per-connection state of the accept step
    template <typename Stream>
    void startReadAcceptRequest(const std::shared_ptr<Listener>& listener,
                                boost::asio::ip::tcp::acceptor& tcpAcceptor,
                                std::shared_ptr<AcceptOp<Stream>> acceptOp);

    /// @brief creates a connection Session using provided web-socket stream object
    /// @param wsStream web-socket stream object which provides as a R/W interface for connection
    /// @param user context, usualy a pointer to the authenticated user object
    /// @param endpointAddress the IP address of the connection endpoint associated with the session
    /// @brief endpointPortNumber the port number of the connection endpoint associated with the session
    /// @return pointer to created Session object
    template <typename Stream>
    std::shared_ptr<Session> createSession(std::shared_ptr<Stream> wsStream,
                                           const std::shared_ptr<void>& userContext,
                                           const std::string& endpointAddress,
                                           const uint16_t& endpointPortNumber);

    /// async operations handler
    std::shared_ptr<boost::asio::io_context> ioContextPtr;

    /// Redirects log calls
    LogCallback logCallback;

    /// callback used to provide newly created Session to outside world, e.g. to protocol handler
    OnNewSessionCallback onNewSessionCallback;

    /// callback which is triggered on authenticaiton step. It should return true for successful authentication and false otherwise.
    OnAuthenticateCallback onAuthenticateCallback;

    /// the ports the server listens on. Held by shared pointer because the accept operations
    /// keep a reference of their own: a listener dropped here survives until the last operation on it
    /// has completed, so stopping the server can release them without waiting
    std::vector<std::shared_ptr<Listener>> listeners;
};

END_NAMESPACE_NATIVE_STREAMING
