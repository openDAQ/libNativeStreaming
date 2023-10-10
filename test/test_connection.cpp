#include <native_streaming/server.hpp>
#include <native_streaming/client.hpp>
#include "test_connection.hpp"

using namespace daq::native_streaming;

TEST_F(ConnectionTest, Connect)
{
    auto server = std::make_shared<Server>(onNewServerSessionCallback, ioContextPtrServer, logCallback);
    server->start(CONNECTION_PORT);

    auto client = std::make_shared<Client>(CONNECTION_HOST,
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           onNewClientSessionCallback,
                                           onClientConnectFailedCallback,
                                           ioContextPtrClient,
                                           logCallback);
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    ASSERT_TRUE(serverSession.operator bool());
    ASSERT_TRUE(clientSession.operator bool());

    serverSession.reset();
    clientSession.reset();
}

TEST_F(ConnectionTest, ConnectCloseServerFirst)
{
    auto server = std::make_shared<Server>(onNewServerSessionCallback, ioContextPtrServer, logCallback);
    server->start(CONNECTION_PORT);

    auto client = std::make_shared<Client>(CONNECTION_HOST,
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           onNewClientSessionCallback,
                                           onClientConnectFailedCallback,
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
    auto server = std::make_shared<Server>(onNewServerSessionCallback, ioContextPtrServer, logCallback);
    server->start(CONNECTION_PORT);

    auto client = std::make_shared<Client>(CONNECTION_HOST,
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           onNewClientSessionCallback,
                                           onClientConnectFailedCallback,
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
    auto server = std::make_shared<Server>(onNewServerSessionCallback, ioContextPtrServer, logCallback);
    server->start(CONNECTION_PORT);

    auto client = std::make_shared<Client>(CONNECTION_HOST,
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           onNewClientSessionCallback,
                                           onClientConnectFailedCallback,
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
    auto server = std::make_shared<Server>(onNewServerSessionCallback, ioContextPtrServer, logCallback);
    server->start(CONNECTION_PORT);

    auto client = std::make_shared<Client>(CONNECTION_HOST,
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           onNewClientSessionCallback,
                                           onClientConnectFailedCallback,
                                           ioContextPtrClient,
                                           logCallback);
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    std::promise<void> readErrorPromise;
    std::future<void> readErrorFuture = readErrorPromise.get_future();
    auto onServerReadErrorCallback = [&readErrorPromise](const boost::system::error_code&) { readErrorPromise.set_value(); };
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
    auto server = std::make_shared<Server>(onNewServerSessionCallback, ioContextPtrServer, logCallback);
    server->start(CONNECTION_PORT);

    auto client = std::make_shared<Client>(CONNECTION_HOST,
                                           std::to_string(CONNECTION_PORT),
                                           CONNECTION_PATH,
                                           onNewClientSessionCallback,
                                           onClientConnectFailedCallback,
                                           ioContextPtrClient,
                                           logCallback);
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    std::promise<void> readErrorPromise;
    std::future<void> readErrorFuture = readErrorPromise.get_future();
    auto onClientReadErrorCallback = [&readErrorPromise](const boost::system::error_code&) { readErrorPromise.set_value(); };
    clientSession->setErrorHandlers(nullptr, onClientReadErrorCallback);

    auto onClientReadCallback = [&, this](const void* data, size_t size) { return ReadTask(); };
    clientSession->scheduleRead(ReadTask(onClientReadCallback, 1));

    serverSession->close(onServerSessionClosedCallback);

    ASSERT_EQ(readErrorFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(serverDisconnectedFuture.wait_for(timeout), std::future_status::ready);

    clientSession->close(onClientSessionClosedCallback);
    ASSERT_EQ(clientDisconnectedFuture.wait_for(timeout), std::future_status::ready);
}
