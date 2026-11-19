#include <native_streaming/server.hpp>
#include <native_streaming/client.hpp>
#include "test_connection.hpp"

using namespace daq::native_streaming;

INSTANTIATE_TEST_SUITE_P(Transport, ConnectionTestP, testing::ValuesIn(transportModes()), transportModeName);

TEST_P(ConnectionTestP, Connect)
{
    auto server = createServer();
    auto client = createClient();
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    ASSERT_TRUE(serverSession.operator bool());
    ASSERT_TRUE(clientSession.operator bool());

    ASSERT_EQ(clientSession->getEndpointAddress(), std::string(CONNECTION_HOST));
    ASSERT_EQ(clientSession->getEndpointPortNumber(), CONNECTION_PORT);

    serverSession.reset();
    clientSession.reset();
}

TEST_P(ConnectionTestP, ConnectTimeoutFailure)
{
    auto server = createDelayedServer(std::chrono::seconds(2));

    std::promise<void> connectionFailedPromise;
    std::future<void> connectionFailedFuture = connectionFailedPromise.get_future();
    onConnectFailedCallback = [&connectionFailedPromise](const boost::system::error_code& ec)
    {
        connectionFailedPromise.set_value();
    };

    auto client = createClient();

    std::chrono::milliseconds timeout(1000);
    client->connect(timeout);

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::timeout);
    ASSERT_EQ(connectionFailedFuture.wait_for(timeout), std::future_status::ready);

    serverSession.reset();
    clientSession.reset();
}

TEST_P(ConnectionTestP, ConnectIPv6)
{
    auto server = createServer();
    auto client = createClient("::1");
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    ASSERT_TRUE(serverSession.operator bool());
    ASSERT_TRUE(clientSession.operator bool());

    ASSERT_EQ(clientSession->getEndpointAddress(), std::string("::1"));
    ASSERT_EQ(clientSession->getEndpointPortNumber(), CONNECTION_PORT);

    serverSession.reset();
    clientSession.reset();
}

TEST_P(ConnectionTestP, ConnectCloseServerFirst)
{
    auto server = createServer();
    auto client = createClient();
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    serverSession->close(onServerSessionClosedCallback);
    clientSession->close(onClientSessionClosedCallback);

    ASSERT_EQ(clientDisconnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverDisconnectedFuture.wait_for(timeout), std::future_status::ready);
}

TEST_P(ConnectionTestP, ConnectCloseClientFirst)
{
    auto server = createServer();
    auto client = createClient();
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    clientSession->close(onClientSessionClosedCallback);
    serverSession->close(onServerSessionClosedCallback);

    ASSERT_EQ(clientDisconnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverDisconnectedFuture.wait_for(timeout), std::future_status::ready);
}

TEST_P(ConnectionTestP, Reconnect)
{
    auto server = createServer();
    auto client = createClient();
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    clientSession->close(onClientSessionClosedCallback);
    serverSession->close(onServerSessionClosedCallback);

    ASSERT_EQ(clientDisconnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverDisconnectedFuture.wait_for(timeout), std::future_status::ready);

    ResetFuturePromise();

    // connect again
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    clientSession->close(onClientSessionClosedCallback);
    serverSession->close(onServerSessionClosedCallback);

    ASSERT_EQ(clientDisconnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverDisconnectedFuture.wait_for(timeout), std::future_status::ready);
}

TEST_P(ConnectionTestP, ServerReadErrorOnDisconnect)
{
    auto server = createServer();
    auto client = createClient();
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    std::promise<void> readErrorPromise;
    std::future<void> readErrorFuture = readErrorPromise.get_future();
    auto onServerReadErrorCallback = [&readErrorPromise](const std::string&, std::shared_ptr<Session>) { readErrorPromise.set_value(); };
    serverSession->setErrorHandlers(nullptr, onServerReadErrorCallback);

    auto onServerReadCallback = [&, this](const void* data, size_t size) { return ReadTask(); };
    serverSession->scheduleRead(ReadTask(onServerReadCallback, 1));

    clientSession->close(onClientSessionClosedCallback);

    ASSERT_EQ(readErrorFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(clientDisconnectedFuture.wait_for(timeout), std::future_status::ready);

    serverSession->close(onServerSessionClosedCallback);
    ASSERT_EQ(serverDisconnectedFuture.wait_for(timeout), std::future_status::ready);
}

TEST_P(ConnectionTestP, ClientReadErrorOnDisconnect)
{
    auto server = createServer();
    auto client = createClient();
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    std::promise<void> readErrorPromise;
    std::future<void> readErrorFuture = readErrorPromise.get_future();
    auto onClientReadErrorCallback = [&readErrorPromise](const std::string&, std::shared_ptr<Session>) { readErrorPromise.set_value(); };
    clientSession->setErrorHandlers(nullptr, onClientReadErrorCallback);

    auto onClientReadCallback = [&, this](const void* data, size_t size) { return ReadTask(); };
    clientSession->scheduleRead(ReadTask(onClientReadCallback, 1));

    serverSession->close(onServerSessionClosedCallback);

    ASSERT_EQ(readErrorFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverDisconnectedFuture.wait_for(timeout), std::future_status::ready);

    clientSession->close(onClientSessionClosedCallback);
    ASSERT_EQ(clientDisconnectedFuture.wait_for(timeout), std::future_status::ready);
}

TEST_P(ConnectionTestP, ConnectionActivityHeartbeat)
{
    const size_t hbPeriodMs = 200;
    const size_t testDurationMs = 2100;
    const size_t pongsCount = testDurationMs / hbPeriodMs + 1;

    const auto heartbeatPeriod = std::chrono::milliseconds(hbPeriodMs);
    auto server = createServer();
    auto client = createClient();
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    size_t clientReceivedPongs = 0;
    auto onClientConnectionAliveCb = [&clientReceivedPongs]() { clientReceivedPongs++; };

    size_t serverReceivedPongs = 0;
    auto onServerConnectionAliveCb = [&serverReceivedPongs]() { serverReceivedPongs++; };

    serverSession->startConnectionActivityMonitoring(onServerConnectionAliveCb, heartbeatPeriod);
    clientSession->startConnectionActivityMonitoring(onClientConnectionAliveCb, heartbeatPeriod);

    // start reading to enable control callbacks
    auto onServerReadCallback = [&, this](const void*, size_t) { return ReadTask(); };
    serverSession->scheduleRead(ReadTask(onServerReadCallback, 1));
    auto onClientReadCallback = [&, this](const void*, size_t) { return ReadTask(); };
    clientSession->scheduleRead(ReadTask(onClientReadCallback, 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(testDurationMs));

    ASSERT_EQ(clientReceivedPongs, pongsCount);
    ASSERT_EQ(serverReceivedPongs, pongsCount);
}
