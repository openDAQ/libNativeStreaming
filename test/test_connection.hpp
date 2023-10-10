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

#include <gtest/gtest.h>
#include <native_streaming/logging.hpp>
#include "test_base.hpp"

#include <future>

BEGIN_NAMESPACE_NATIVE_STREAMING

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

        onServerSessionClosedCallback = [this](const boost::system::error_code& ec)
        {
            if (ec)
            {
                ADD_FAILURE() << "Server session closing failed: " << ec.what();
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
                ADD_FAILURE() << "Client session closing failed: " << ec.what();
            }
            clientSession.reset();
            clientDisconnectedPromise.set_value();
        };

        onClientConnectFailedCallback = [this](const boost::system::error_code& ec)
        { ADD_FAILURE() << "Client connection failed: " << ec.what(); };
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

    OnCompleteCallback onClientConnectFailedCallback;
};

END_NAMESPACE_NATIVE_STREAMING
