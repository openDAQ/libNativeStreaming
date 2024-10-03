#include <native_streaming/server.hpp>
#include <native_streaming/client.hpp>
#include "test_connection.hpp"

using namespace daq::native_streaming;

TEST_F(ConnectionTest, Connect)
{
    auto server = std::make_shared<Server>(onNewServerSessionCallback, onAuthenticateCallback, ioContextPtrServer, logCallback);
    server->start(CONNECTION_PORT);

    auto client = std::make_shared<Client>(CONNECTION_HOST,
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           authentication,
                                           onNewClientSessionCallback,
                                           onResolveFailedCallback,
                                           onConnectFailedCallback,
                                           onHandshakeFailedCallback,
                                           ioContextPtrClient,
                                           logCallback);
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    ASSERT_TRUE(serverSession.operator bool());
    ASSERT_TRUE(clientSession.operator bool());

    ASSERT_EQ(clientSession->getEndpointAddress(), std::string(CONNECTION_HOST) + std::string(":") + std::to_string(CONNECTION_PORT));

    serverSession.reset();
    clientSession.reset();
}

TEST_F(ConnectionTest, ConnectIPv6)
{
    auto server = std::make_shared<Server>(onNewServerSessionCallback, onAuthenticateCallback, ioContextPtrServer, logCallback);
    server->start(CONNECTION_PORT);

    auto client = std::make_shared<Client>("::1",
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           authentication,
                                           onNewClientSessionCallback,
                                           onResolveFailedCallback,
                                           onConnectFailedCallback,
                                           onHandshakeFailedCallback,
                                           ioContextPtrClient,
                                           logCallback);
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    ASSERT_TRUE(serverSession.operator bool());
    ASSERT_TRUE(clientSession.operator bool());

    ASSERT_EQ(clientSession->getEndpointAddress(), std::string("::1") + std::string(":") + std::to_string(CONNECTION_PORT));

    serverSession.reset();
    clientSession.reset();
}

TEST_F(ConnectionTest, ConnectCloseServerFirst)
{
    auto server = std::make_shared<Server>(onNewServerSessionCallback, onAuthenticateCallback, ioContextPtrServer, logCallback);
    server->start(CONNECTION_PORT);

    auto client = std::make_shared<Client>(CONNECTION_HOST,
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           authentication,
                                           onNewClientSessionCallback,
                                           onResolveFailedCallback,
                                           onConnectFailedCallback,
                                           onHandshakeFailedCallback,
                                           ioContextPtrClient,
                                           logCallback);
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    serverSession->close(onServerSessionClosedCallback);
    clientSession->close(onClientSessionClosedCallback);

    ASSERT_EQ(clientDisconnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverDisconnectedFuture.wait_for(timeout), std::future_status::ready);
}

TEST_F(ConnectionTest, ConnectCloseClientFirst)
{
    auto server = std::make_shared<Server>(onNewServerSessionCallback, onAuthenticateCallback, ioContextPtrServer, logCallback);
    server->start(CONNECTION_PORT);

    auto client = std::make_shared<Client>(CONNECTION_HOST,
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           authentication,
                                           onNewClientSessionCallback,
                                           onResolveFailedCallback,
                                           onConnectFailedCallback,
                                           onHandshakeFailedCallback,
                                           ioContextPtrClient,
                                           logCallback);
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    clientSession->close(onClientSessionClosedCallback);
    serverSession->close(onServerSessionClosedCallback);

    ASSERT_EQ(clientDisconnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverDisconnectedFuture.wait_for(timeout), std::future_status::ready);
}

TEST_F(ConnectionTest, Reconnect)
{
    auto server = std::make_shared<Server>(onNewServerSessionCallback, onAuthenticateCallback, ioContextPtrServer, logCallback);
    server->start(CONNECTION_PORT);

    auto client = std::make_shared<Client>(CONNECTION_HOST,
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           authentication,
                                           onNewClientSessionCallback,
                                           onResolveFailedCallback,
                                           onConnectFailedCallback,
                                           onHandshakeFailedCallback,
                                           ioContextPtrClient,
                                           logCallback);
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

TEST_F(ConnectionTest, ServerReadErrorOnDisconnect)
{
    auto server = std::make_shared<Server>(onNewServerSessionCallback, onAuthenticateCallback, ioContextPtrServer, logCallback);
    server->start(CONNECTION_PORT);

    auto client = std::make_shared<Client>(CONNECTION_HOST,
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           authentication,
                                           onNewClientSessionCallback,
                                           onResolveFailedCallback,
                                           onConnectFailedCallback,
                                           onHandshakeFailedCallback,
                                           ioContextPtrClient,
                                           logCallback);
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

TEST_F(ConnectionTest, ClientReadErrorOnDisconnect)
{
    auto server = std::make_shared<Server>(onNewServerSessionCallback, onAuthenticateCallback, ioContextPtrServer, logCallback);
    server->start(CONNECTION_PORT);

    auto client = std::make_shared<Client>(CONNECTION_HOST,
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           authentication,
                                           onNewClientSessionCallback,
                                           onResolveFailedCallback,
                                           onConnectFailedCallback,
                                           onHandshakeFailedCallback,
                                           ioContextPtrClient,
                                           logCallback);
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

TEST_F(ConnectionTest, ConnectionActivityHeartbeat)
{
    const size_t hbPeriodMs = 200;
    const size_t testDurationMs = 2100;
    const size_t pongsCount = testDurationMs / hbPeriodMs + 1;

    const auto heartbeatPeriod = std::chrono::milliseconds(hbPeriodMs);
    auto server = std::make_shared<Server>(onNewServerSessionCallback, onAuthenticateCallback, ioContextPtrServer, logCallback);
    server->start(CONNECTION_PORT);

    auto client = std::make_shared<Client>(CONNECTION_HOST,
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           authentication,
                                           onNewClientSessionCallback,
                                           onResolveFailedCallback,
                                           onConnectFailedCallback,
                                           onHandshakeFailedCallback,
                                           ioContextPtrClient,
                                           logCallback);
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
