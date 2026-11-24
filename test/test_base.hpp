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

#include <mutex>
#include <string>

BEGIN_NAMESPACE_NATIVE_STREAMING

class TestBase : public testing::Test
{
protected:
    TestBase()
        : ioContextPtrServer(std::make_shared<boost::asio::io_context>())
        , workGuardServer(ioContextPtrServer->get_executor())
        , ioContextPtrClient(std::make_shared<boost::asio::io_context>())
        , workGuardClient(ioContextPtrClient->get_executor())
    {
        NS_LOG_T("Start server asynchronous io context");
        execThreadServer = std::thread(
            [this]()
            {
                runIoContext(*this->ioContextPtrServer, "server");
                NS_LOG_T("server io context stopped");
            });

        NS_LOG_T("Start Client asynchronous io context");
        execThreadClient = std::thread(
            [this]()
            {
                runIoContext(*this->ioContextPtrClient, "client");
                NS_LOG_T("Client io context stopped");
            });
    }

    ~TestBase()
    {
        ReportIoContextException();

        NS_LOG_T("Stop server asynchronous io context");
        ioContextPtrServer->stop();
        if (execThreadServer.joinable())
        {
            execThreadServer.join();
            NS_LOG_T("join Server exec thread");
        }

        NS_LOG_T("Stop Client asynchronous io context");
        ioContextPtrClient->stop();
        if (execThreadClient.joinable())
        {
            execThreadClient.join();
            NS_LOG_T("join Client exec thread");
        }
    }

    /// runs an io context, turning an exception escaping an asynchronous operation into a test
    /// failure. Without this the exception would leave the thread and terminate the process, which
    /// reads as a silent abort rather than as a failing test
    void runIoContext(boost::asio::io_context& ioContext, const char* name)
    {
        try
        {
            ioContext.run();
        }
        catch (const std::exception& e)
        {
            std::lock_guard<std::mutex> lock(ioContextExceptionSync);
            if (ioContextException.empty())
                ioContextException = std::string(name) + " io context: " + e.what();
        }
        catch (...)
        {
            std::lock_guard<std::mutex> lock(ioContextExceptionSync);
            if (ioContextException.empty())
                ioContextException = std::string(name) + " io context: unknown exception";
        }
    }

    /// fails the test if an asynchronous operation let an exception escape. Called from the
    /// test thread, since gtest assertions may not be used on the io context threads
    void ReportIoContextException()
    {
        std::lock_guard<std::mutex> lock(ioContextExceptionSync);
        if (!ioContextException.empty())
            ADD_FAILURE() << "Exception escaped an asynchronous operation: " << ioContextException;
    }

    /// Redirects log calls to internal library logger implementation
    const LogCallback logCallback = Logging::logCallback();

    /// guards the message of the first exception which escaped an asynchronous operation
    std::mutex ioContextExceptionSync;
    std::string ioContextException;

    /// connection / disconnection timeout
    const std::chrono::seconds timeout = std::chrono::seconds(5);

    const uint16_t CONNECTION_PORT = 7420;
    const std::string CONNECTION_HOST = "127.0.0.1";
    const std::string CONNECTION_PATH = "/";

    /// async operations handler
    std::shared_ptr<boost::asio::io_context> ioContextPtrClient;
    /// prevents boost::asio::io_context::run() from returning when there is no more async operations pending
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> workGuardClient;
    /// async operations runner thread
    std::thread execThreadClient;

    /// async operations handler
    std::shared_ptr<boost::asio::io_context> ioContextPtrServer;
    /// prevents boost::asio::io_context::run() from returning when there is no more async operations pending
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> workGuardServer;
    /// async operations runner thread
    std::thread execThreadServer;
};

END_NAMESPACE_NATIVE_STREAMING
