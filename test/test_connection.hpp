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

#include <gtest/gtest.h>
#include <native_streaming/logging.hpp>
#include "test_base.hpp"

#include <future>

BEGIN_NAMESPACE_NATIVE_STREAMING

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
    void onAcceptTcpConnection(boost::asio::ip::tcp::acceptor& tcpAcceptor,
                               const boost::system::error_code& ec,
                               boost::asio::ip::tcp::socket&& socket) override
    {
        if (!ec)
            std::this_thread::sleep_for(delayAfterTcpConnectionAccepted);
        Server::onAcceptTcpConnection(tcpAcceptor, ec, std::move(socket));
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
};

END_NAMESPACE_NATIVE_STREAMING
