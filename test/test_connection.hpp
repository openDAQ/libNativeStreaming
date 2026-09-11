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

#include <gtest/gtest.h>
#include <native_streaming/logging.hpp>
#include <native_streaming/server.hpp>
#include <native_streaming/client.hpp>
#include "test_base.hpp"
#include "test_secrets.hpp"

#include <future>
#include <string>
#include <vector>

BEGIN_NAMESPACE_NATIVE_STREAMING

enum class TransportMode
{
    Plaintext,
    Tls
};

/// the modes to instantiate a transport-parameterized suite for
inline std::vector<TransportMode> transportModes()
{
    std::vector<TransportMode> modes{TransportMode::Plaintext};
#if NATIVE_STREAMING_ENABLE_TLS
    modes.push_back(TransportMode::Tls);
#endif
    return modes;
}

/// names the instantiations of a transport-parameterized suite
inline std::string transportModeName(const testing::TestParamInfo<TransportMode>& info)
{
    return info.param == TransportMode::Tls ? "Tls" : "Plaintext";
}

/// Modified Server with artificial delays introduced
class MockServer : public Server
{
public:
    MockServer(OnNewSessionCallback onNewSessionCallback,
               OnAuthenticateCallback onAuthenticateCallback,
               std::shared_ptr<boost::asio::io_context> ioContextPtr,
               LogCallback logCallback,
               std::chrono::seconds delayAfterTcpConnectionAccepted)
        : Server(onNewSessionCallback, onAuthenticateCallback, ioContextPtr, logCallback)
        , delayAfterTcpConnectionAccepted(delayAfterTcpConnectionAccepted)
    {
    }

protected:
    void onAcceptTcpConnection(const std::shared_ptr<Listener>& listener,
                               boost::asio::ip::tcp::acceptor& tcpAcceptor,
                               const boost::system::error_code& ec,
                               boost::asio::ip::tcp::socket&& socket) override
    {
        if (!ec)
            std::this_thread::sleep_for(delayAfterTcpConnectionAccepted);
        Server::onAcceptTcpConnection(listener, tcpAcceptor, ec, std::move(socket));
    }

private:
    std::chrono::seconds delayAfterTcpConnectionAccepted;
};

class ConnectionTest : public TestBase
{
public:
    ConnectionTest()
    {
        onNewServerSessionCallback = [this](std::shared_ptr<Session> session)
        {
            serverSession = session;
            serverConnectedPromise.set_value();
        };

        onAuthenticateCallback = [this](const Authentication& authentication, std::shared_ptr<void>& userContextOut) 
        {
            return true; 
        };

        onServerSessionClosedCallback = [this](const boost::system::error_code& ec)
        {
            if (ec)
            {
                ADD_FAILURE() << "Server session closing failed: " << ec.message();
            }
            serverSession.reset();
            serverDisconnectedPromise.set_value();
        };

        onNewClientSessionCallback = [this](std::shared_ptr<Session> session)
        {
            clientSession = session;
            clientConnectedPromise.set_value();
        };

        onClientSessionClosedCallback = [this](const boost::system::error_code& ec)
        {
            if (ec)
            {
                ADD_FAILURE() << "Client session closing failed: " << ec.message();
            }
            clientSession.reset();
            clientDisconnectedPromise.set_value();
        };

        onResolveFailedCallback = [this](const boost::system::error_code& ec)
        { ADD_FAILURE() << "Resolve failed: " << ec.message(); };

        onConnectFailedCallback = [this](const boost::system::error_code& ec)
        { ADD_FAILURE() << "Connection failed: " << ec.message(); };

        onHandshakeFailedCallback = [this](const boost::system::error_code& ec)
        { ADD_FAILURE() << "Handshake failed: " << ec.message(); };
    }

protected:
    /// @brief creates a server for the transport this test runs over and starts it listening
    /// @param port the port to listen on
    /// @return the started server
    std::shared_ptr<Server> createServer(uint16_t port = 0)
    {
        auto server = std::make_shared<Server>(onNewServerSessionCallback, onAuthenticateCallback, ioContextPtrServer, logCallback);
        startServer(server, port == 0 ? CONNECTION_PORT : port);
        return server;
    }

    /// @brief creates a server which delays before servicing an accepted connection, and starts it
    /// @param delayAfterTcpConnectionAccepted how long to sleep once a connection has been accepted
    /// @return the started server
    std::shared_ptr<Server> createDelayedServer(std::chrono::seconds delayAfterTcpConnectionAccepted)
    {
        auto server = std::make_shared<MockServer>(
            onNewServerSessionCallback, onAuthenticateCallback, ioContextPtrServer, logCallback, delayAfterTcpConnectionAccepted);
        startServer(server, CONNECTION_PORT);
        return server;
    }

    /// @brief starts the given server on the given port, over the transport this test runs over
    void startServer(const std::shared_ptr<Server>& server, uint16_t port)
    {
#if NATIVE_STREAMING_ENABLE_TLS
        if (transportMode == TransportMode::Tls)
        {
            const auto ec = server->startTls(port, test_secrets::ServerCert, test_secrets::ServerKey, serverCaFile);
            ASSERT_FALSE(ec) << "Failed to start the TLS listener: " << ec.message();
            return;
        }
#endif
        const auto ec = server->start(port);
        ASSERT_FALSE(ec) << "Failed to start the listener: " << ec.message();
    }

    /// @brief creates a client for the transport this test runs over. The callbacks it is given are
    /// the fixture's members as they stand at this point, so a test which overrides one has to do so
    /// before calling this
    /// @param host the address to connect to
    /// @param port the port to connect to
    /// @return the client, not yet connected
    std::shared_ptr<Client> createClient(const std::string& host = std::string(), uint16_t port = 0)
    {
        const auto connectHost = host.empty() ? CONNECTION_HOST : host;
        const auto connectPort = std::to_string(port == 0 ? CONNECTION_PORT : port);

#if NATIVE_STREAMING_ENABLE_TLS
        if (transportMode == TransportMode::Tls)
        {
            ClientTlsConfig tlsConfig;
            tlsConfig.caFile = clientCaFile;
            tlsConfig.certFile = clientCertFile;
            tlsConfig.keyFile = clientKeyFile;
            tlsConfig.verifyServer = verifyServer;

            return std::make_shared<Client>(connectHost,
                                            connectPort,
                                            CONNECTION_PATH,
                                            authentication,
                                            onNewClientSessionCallback,
                                            onResolveFailedCallback,
                                            onConnectFailedCallback,
                                            onHandshakeFailedCallback,
                                            ioContextPtrClient,
                                            logCallback,
                                            tlsConfig,
                                            onTlsHandshakeFailedCallback);
        }
#endif

        return std::make_shared<Client>(connectHost,
                                        connectPort,
                                        CONNECTION_PATH,
                                        authentication,
                                        onNewClientSessionCallback,
                                        onResolveFailedCallback,
                                        onConnectFailedCallback,
                                        onHandshakeFailedCallback,
                                        ioContextPtrClient,
                                        logCallback);
    }

    void ResetFuturePromise()
    {
        // reset promise / future
        serverConnectedPromise = std::promise<void>();
        serverConnectedFuture = serverConnectedPromise.get_future();
        serverDisconnectedPromise = std::promise<void>();
        serverDisconnectedFuture = serverDisconnectedPromise.get_future();
        clientConnectedPromise = std::promise<void>();
        clientConnectedFuture = clientConnectedPromise.get_future();
        clientDisconnectedPromise = std::promise<void>();
        clientDisconnectedFuture = clientDisconnectedPromise.get_future();
    }

    void SetUp() override
    {
        ResetFuturePromise();

        serverSession.reset();
        clientSession.reset();
    }

    void TearDown() override
    {
    }

    std::promise<void> serverConnectedPromise;
    std::future<void> serverConnectedFuture;
    std::shared_ptr<Session> serverSession;
    OnNewSessionCallback onNewServerSessionCallback;
    OnAuthenticateCallback onAuthenticateCallback;
    std::promise<void> serverDisconnectedPromise;
    std::future<void> serverDisconnectedFuture;
    OnCompleteCallback onServerSessionClosedCallback;

    std::promise<void> clientConnectedPromise;
    std::future<void> clientConnectedFuture;
    std::shared_ptr<Session> clientSession;
    OnNewSessionCallback onNewClientSessionCallback;
    std::promise<void> clientDisconnectedPromise;
    std::future<void> clientDisconnectedFuture;
    OnCompleteCallback onClientSessionClosedCallback;

    OnCompleteCallback onConnectFailedCallback;
    OnCompleteCallback onResolveFailedCallback;
    OnCompleteCallback onHandshakeFailedCallback;
    Authentication authentication;

    TransportMode transportMode = TransportMode::Plaintext;

#if NATIVE_STREAMING_ENABLE_TLS

    OnCompleteCallback onTlsHandshakeFailedCallback;

    std::string clientCaFile = test_secrets::CaCert;
    std::string clientCertFile;
    std::string clientKeyFile;
    bool verifyServer = true;
    std::string serverCaFile;

#endif
};

#if NATIVE_STREAMING_ENABLE_TLS

class ConnectionTestTlsDefaults : public ConnectionTest
{
public:
    ConnectionTestTlsDefaults()
    {
        onTlsHandshakeFailedCallback = [](const boost::system::error_code& ec)
        { ADD_FAILURE() << "TLS handshake failed: " << ec.message(); };
    }
};

using ConnectionTestBase = ConnectionTestTlsDefaults;

#else

using ConnectionTestBase = ConnectionTest;

#endif

class ConnectionTestP : public ConnectionTestBase, public testing::WithParamInterface<TransportMode>
{
protected:
    void SetUp() override
    {
        transportMode = GetParam();
        ConnectionTest::SetUp();
    }
};

END_NAMESPACE_NATIVE_STREAMING
