#include <native_streaming/server.hpp>
#include <gtest/gtest.h>
#include <native_streaming/logging.hpp>
#include "test_base.hpp"

using namespace daq::native_streaming;

class ServerTest : public TestBase
{
public:
    /// Drops created session
    const OnNewSessionCallback onUnusedSessionCallback = [](std::shared_ptr<Session>) {};
};

TEST_F(ServerTest, ServerCreate)
{
    ASSERT_NO_THROW(Server(onUnusedSessionCallback, ioContextPtrServer, logCallback));
}

TEST_F(ServerTest, ServerStartStop)
{
    const unsigned int cycleCount = 10;

    auto server = std::make_shared<Server>(onUnusedSessionCallback, ioContextPtrServer, logCallback);

    for (unsigned int counter = 0; counter < cycleCount; ++counter)
    {
        ASSERT_EQ(server->start(CONNECTION_PORT), boost::system::error_code());
        server->stop();
    }
}

TEST_F(ServerTest, ServerStartTwice)
{
    auto server = std::make_shared<Server>(onUnusedSessionCallback, ioContextPtrServer, logCallback);

    ASSERT_EQ(server->start(CONNECTION_PORT), boost::system::error_code());
    ASSERT_NE(server->start(CONNECTION_PORT), boost::system::error_code());
}

TEST_F(ServerTest, DISABLED_MultipleServersSamePort)
{
    auto server1 = std::make_shared<Server>(onUnusedSessionCallback, ioContextPtrServer, logCallback);
    auto server2 = std::make_shared<Server>(onUnusedSessionCallback, ioContextPtrServer, logCallback);

    ASSERT_EQ(server1->start(CONNECTION_PORT), boost::system::error_code());
    ASSERT_NE(server2->start(CONNECTION_PORT), boost::system::error_code());
}

TEST_F(ServerTest, MultipleServersDiffPorts)
{
    auto server1 = std::make_shared<Server>(onUnusedSessionCallback, ioContextPtrServer, logCallback);
    auto server2 = std::make_shared<Server>(onUnusedSessionCallback, ioContextPtrServer, logCallback);

    ASSERT_EQ(server1->start(CONNECTION_PORT), boost::system::error_code());
    ASSERT_EQ(server2->start(CONNECTION_PORT + 1), boost::system::error_code());
}
