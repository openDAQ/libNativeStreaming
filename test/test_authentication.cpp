#include <gtest/gtest.h>
#include <native_streaming/client.hpp>
#include <native_streaming/server.hpp>

#include "test_connection.hpp"

using namespace daq::native_streaming;

class AuthenticationTest : public ConnectionTest
{
public:
    std::shared_ptr<Server> createServer(const OnAuthenticateCallback& onAuthenticateCallback)
    {
        auto wrappedAuthenticateCallback = [this, onAuthenticateCallback](const Authentication& authentication)
        {
            const bool authenticated = onAuthenticateCallback(authentication);

            // on failed authentication we should stop waiting for server session, only for testing purposes
            if (!authenticated)
                serverConnectedPromise.set_value();

            return authenticated;
        };

        auto server = std::make_shared<Server>(onNewServerSessionCallback, wrappedAuthenticateCallback, ioContextPtrServer, logCallback);
        return server;
    }

    std::shared_ptr<Client> createClient(const Authentication& authentication)
    {
        // on failed authentication we should stop waiting for client session
        auto onHandshakeFailedCallback = [this](const boost::system::error_code& e) { clientConnectedPromise.set_value(); };

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

        return client;
    }

    void waitForConnection()
    {
        clientConnectedFuture.wait_for(timeout);
        serverConnectedFuture.wait_for(timeout);
    }

    void resetSessions()
    {
        ResetFuturePromise();
        serverSession.reset();
        clientSession.reset();
    }
};


TEST_F(AuthenticationTest, AllowAll)
{
    auto authCallback = [](const Authentication& authentication) { return true; };

    auto server = createServer(authCallback);
    server->start(CONNECTION_PORT);

    auto client = createClient(Authentication("tomaz", "tomaz"));
    client->connect();

    waitForConnection();
    ASSERT_TRUE(serverSession.operator bool());
    ASSERT_TRUE(clientSession.operator bool());
    resetSessions();

    client = createClient(Authentication());
    client->connect();

    waitForConnection();
    ASSERT_TRUE(serverSession.operator bool());
    ASSERT_TRUE(clientSession.operator bool());
}

TEST_F(AuthenticationTest, DenyAll)
{
    auto authCallback = [this](const Authentication& authentication) { return false; };

    auto server = createServer(authCallback);
    server->start(CONNECTION_PORT);

    auto client = createClient(Authentication("tomaz", "tomaz"));
    client->connect();

    waitForConnection();
    ASSERT_FALSE(serverSession.operator bool());
    ASSERT_FALSE(clientSession.operator bool());
    resetSessions();

    client = createClient(Authentication());
    client->connect();

    waitForConnection();
    ASSERT_FALSE(serverSession.operator bool());
    ASSERT_FALSE(clientSession.operator bool());
}

TEST_F(AuthenticationTest, AllowCorrect)
{
    auto authCallback = [this](const Authentication& authentication)
    {
        return authentication.getUsername() == "jure" && authentication.getPassword() == "jure123";
    };

    auto server = createServer(authCallback);
    server->start(CONNECTION_PORT);

    auto client = createClient(Authentication("jure", "jure123"));
    client->connect();

    waitForConnection();
    ASSERT_TRUE(serverSession.operator bool());
    ASSERT_TRUE(clientSession.operator bool());
    resetSessions();

    client = createClient(Authentication("jure", "wrongPass"));
    client->connect();

    waitForConnection();
    ASSERT_FALSE(serverSession.operator bool());
    ASSERT_FALSE(clientSession.operator bool());
    resetSessions();

    client = createClient(Authentication());
    client->connect();

    waitForConnection();
    ASSERT_FALSE(serverSession.operator bool());
    ASSERT_FALSE(clientSession.operator bool());
}

TEST_F(AuthenticationTest, AllowOnlyAnonymous)
{
    auto authCallback = [this](const Authentication& authentication)
    {
        return authentication.getType() == AuthenticationType::Anonymous;
    };

    auto server = createServer(authCallback);
    server->start(CONNECTION_PORT);

    auto client = createClient(Authentication("jure", "jure123"));
    client->connect();

    waitForConnection();
    ASSERT_FALSE(serverSession.operator bool());
    ASSERT_FALSE(clientSession.operator bool());
    resetSessions();

    client = createClient(Authentication());
    client->connect();

    waitForConnection();
    ASSERT_TRUE(serverSession.operator bool());
    ASSERT_TRUE(clientSession.operator bool());
}
