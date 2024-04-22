#include <native_streaming/client.hpp>
#include <gtest/gtest.h>
#include <native_streaming/logging.hpp>
#include "test_base.hpp"

#include <future>

using namespace daq::native_streaming;

class ClientTest : public TestBase
{
public:
    void SetUp() override
    {
        onUnusedSessionCallback = [](std::shared_ptr<Session>) {};
        clientConnectFailedPromise = std::promise<void>();
        clientConnectFailedFuture = clientConnectFailedPromise.get_future();
        onResolveFailedCallback = [this](const boost::system::error_code&) { clientConnectFailedPromise.set_value(); };
        onConnectFailedCallback = [this](const boost::system::error_code&) { clientConnectFailedPromise.set_value(); };
        onHandshakeFailedCallback = [this](const boost::system::error_code&) { clientConnectFailedPromise.set_value(); };
    }

    void TearDown() override
    {
    }

    std::promise<void> clientConnectFailedPromise;
    std::future<void> clientConnectFailedFuture;
    OnCompleteCallback onResolveFailedCallback;
    OnCompleteCallback onConnectFailedCallback;
    OnCompleteCallback onHandshakeFailedCallback;

    /// Drops created session
    OnNewSessionCallback onUnusedSessionCallback;
};

TEST_F(ClientTest, Create)
{
    ASSERT_NO_THROW(Client(CONNECTION_HOST,
                           std::to_string(CONNECTION_PORT),
                           CONNECTION_PATH,
                           onUnusedSessionCallback,
                           onResolveFailedCallback,
                           onConnectFailedCallback,
                           onHandshakeFailedCallback,
                           ioContextPtrClient,
                           logCallback));
}

TEST_F(ClientTest, CreateWithIPv6)
{
    ASSERT_NO_THROW(Client("[::1]",
                           std::to_string(CONNECTION_PORT),
                           CONNECTION_PATH,
                           onUnusedSessionCallback,
                           onResolveFailedCallback,
                           onConnectFailedCallback,
                           onHandshakeFailedCallback,
                           ioContextPtrClient,
                           logCallback));
}

TEST_F(ClientTest, ConnectFailed)
{
    auto client = std::make_shared<Client>(CONNECTION_HOST,
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           onUnusedSessionCallback,
                                           onResolveFailedCallback,
                                           onConnectFailedCallback,
                                           onHandshakeFailedCallback,
                                           ioContextPtrClient,
                                           logCallback);
    client->connect();
    ASSERT_EQ(clientConnectFailedFuture.wait_for(timeout), std::future_status::ready);
}
