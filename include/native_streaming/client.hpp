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

#if NATIVE_STREAMING_ENABLE_TLS

/// @brief describes how a client establishes a TLS-encrypted connection.
/// Only file paths are named here, and the SSL context is built from them inside the library, see
/// tls.hpp: the constructor taking this configuration carries no OpenSSL type, and a missing or
/// malformed file is reported by that constructor rather than on the first connection attempt.
struct ClientTlsConfig
{
    /// @brief path to a PEM file of trusted CA certificates used to verify the server.
    /// Required unless verifyServer is false, in which case it is ignored
    std::string caFile;

    /// @brief path to the client certificate chain (PEM), for mutual TLS. Optional, but if set
    /// keyFile must be set as well. Ignored when verifyServer is false
    std::string certFile;

    /// @brief path to the client private key (PEM), for mutual TLS. Optional, but if set certFile
    /// must be set as well. Ignored when verifyServer is false
    std::string keyFile;

    /// @brief when false, the connection is encrypted but the server is not authenticated: whatever
    /// certificate it presents is accepted, which leaves no protection against an active
    /// man-in-the-middle. Use it only where the server is trusted by other means
    bool verifyServer = true;
};

#endif

/// @brief Connects to specified server address, creates and returns new Session via callback when connection established
class Client : public std::enable_shared_from_this<Client>
{
public:
    /// @brief constructs a client which connects in plaintext
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

#if NATIVE_STREAMING_ENABLE_TLS

    /// @brief constructs a client which connects over TLS
    /// @param tlsConfig names the files the SSL context is built from
    /// @param onTlsHandshakeFailCallback callback to be called if a handshake fails with an error
    /// raised by the TLS layer, which means the server was reached but the two could not agree on
    /// trust - unlike the other failures, retrying such a connection with the same secrets is
    /// pointless. If not set, such failures are reported through onHandshakeFailCallback
    /// @throw std::invalid_argument tlsConfig is inconsistent, e.g. it names a client certificate
    /// without a key
    /// @throw boost::system::system_error a certificate or key file named by tlsConfig is missing or
    /// malformed
    explicit Client(const std::string& host,
                    const std::string& port,
                    const std::string& path,
                    const Authentication& authentication,
                    OnNewSessionCallback onNewSessionCallback,
                    OnCompleteCallback onResolveFailCallback,
                    OnCompleteCallback onConnectFailCallback,
                    OnCompleteCallback onHandshakeFailCallback,
                    std::shared_ptr<boost::asio::io_context> ioContextPtr,
                    LogCallback logCallback,
                    const ClientTlsConfig& tlsConfig,
                    OnCompleteCallback onTlsHandshakeFailCallback = nullptr);

#endif

    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    /// @brief initiates asynchronous connection to remote server. Connection procedure contains next steps:
    /// 1. resolve address specified in Client constructor
    /// 2. connect to server using endpoind resolved from address
    /// 3. do TLS handshake, if the client was constructed with a TLS configuration
    /// 4. do web-socket handshake
    /// @param timeout duration in milliseconds after which a connection attempt is considered failed
    /// and will be canceled
    void connect(const std::chrono::milliseconds& timeout = std::chrono::milliseconds(1000));

private:
    void onConnectionTimeout(const boost::system::error_code& ec);

    /// @brief callback called when resolving of remote server address is finished
    /// @param ec error_code object indicates resolving failure
    /// @param results represents connection endpoint resolved from address
    void onResolve(const boost::beast::error_code& ec, boost::asio::ip::tcp::resolver::results_type results);

    /// @brief starts connecting to the resolved endpoint with the given stream
    /// @param wsStream websocket stream object associated with connection
    /// @param results represents connection endpoint resolved from address
    template <typename Stream>
    void startConnect(std::shared_ptr<Stream> wsStream, const boost::asio::ip::tcp::resolver::results_type& results);

    /// @brief callback called when connection to remote server is finished
    /// @param ec error_code object indicates connection failure
    /// @param wsStream websocket stream object associated with connection
    template <typename Stream>
    void onConnect(const boost::system::error_code& ec, std::shared_ptr<Stream> wsStream);

#if NATIVE_STREAMING_ENABLE_TLS

    /// @brief callback called when the TLS handshake finished for new connection
    /// @param ec error_code object indicates handshake failure
    /// @param wsStream websocket stream object associated with connection
    template <typename Stream>
    void onTlsHandshake(const boost::system::error_code& ec, std::shared_ptr<Stream> wsStream);

    /// @brief starts the TLS handshake
    /// @param wsStream websocket stream object associated with connection
    template <typename Stream>
    void startTlsHandshake(std::shared_ptr<Stream> wsStream);

#endif

    /// @brief starts the web-socket handshake, the last step of the connection procedure
    /// @param wsStream websocket stream object associated with connection
    template <typename Stream>
    void startWebsocketHandshake(std::shared_ptr<Stream> wsStream);

    /// @brief reports a failed handshake to the caller
    /// @param ec error_code object describing the failure
    void reportHandshakeFailure(const boost::system::error_code& ec);

    /// @brief callback called when web-socket handshake finished for new connection
    /// @param ec error_code object indicates handshake failure
    /// @param wsStream websocket stream object associated with connection
    template <typename Stream>
    void onUpgradeConnection(const boost::system::error_code& ec, std::shared_ptr<Stream> wsStream);

    /// @brief creates a connection Session using provided web-socket stream object
    /// @param wsStream web-socket stream object which provides as a R/W interface for connection
    /// @param endpointAddress the IP address of the connection endpoint associated with the session
    /// @brief endpointPortNumber the port number of the connection endpoint associated with the session
    /// @return pointer to created Session object
    template <typename Stream>
    std::shared_ptr<Session> createSession(std::shared_ptr<Stream> wsStream,
                                           const std::string& endpointAddress,
                                           const uint16_t& endpointPortNumber);

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

    /// @brief a structure holding authentication information
    Authentication authentication;

    /// @brief The TCP resolver
    boost::asio::ip::tcp::resolver resolver;

    /// @brief Handles the timeout for a connection attempt
    boost::asio::steady_timer connectionTimeoutTimer;

    /// @brief plaintext websocket stream object associated with connection, provides as a R/W
    /// interface for connection. Stays null when the connection is TLS-encrypted, and is reset once
    /// the session takes the stream over
    std::shared_ptr<WebsocketStream> websocketStream;

#if NATIVE_STREAMING_ENABLE_TLS

    /// @brief the SSL context the encrypted streams are created with. Null when the client was
    /// constructed to connect in plaintext
    std::unique_ptr<boost::asio::ssl::context> sslContext;

    /// @brief encrypted websocket stream object associated with connection. Stays null when the
    /// connection is established in plaintext, and is reset once the session takes the stream over
    std::shared_ptr<TlsWebsocketStream> tlsWebsocketStream;

#endif

    /// @brief callback used to provide newly created Session to outside world, e.g. to protocol handler
    OnNewSessionCallback onNewSessionCallback;

    /// @brief callback to be called if resolving address of remote server is failed
    OnCompleteCallback onResolveFailCallback;

    /// @brief callback to be called if connection to remote server is failed
    OnCompleteCallback onConnectFailCallback;

    /// @brief callback to be called if handshake with a remote server is failed
    OnCompleteCallback onHandshakeFailCallback;

#if NATIVE_STREAMING_ENABLE_TLS

    /// @brief callback to be called if a handshake fails with an error raised by the TLS layer.
    /// May be unset, in which case such failures are reported through onHandshakeFailCallback
    OnCompleteCallback onTlsHandshakeFailCallback;

#endif
};

END_NAMESPACE_NATIVE_STREAMING
