#include <native_streaming/server.hpp>
#include <native_streaming/client.hpp>
#include "test_connection.hpp"

#if NATIVE_STREAMING_ENABLE_TLS

using namespace daq::native_streaming;

class TlsConnectionTest : public ConnectionTest
{
protected:
    void SetUp() override
    {
        transportMode = TransportMode::Tls;
        ConnectionTest::SetUp();

        // the failures below are what most of these tests are about, so they are recorded rather
        // than reported, and each test states which one it expects
        onHandshakeFailedCallback = [this](const boost::system::error_code& ec) { recordFailure("handshake", ec); };
        onTlsHandshakeFailedCallback = [this](const boost::system::error_code& ec) { recordFailure("tls", ec); };
        onConnectFailedCallback = [this](const boost::system::error_code& ec) { recordFailure("connect", ec); };
    }

    void recordFailure(const std::string& kind, const boost::system::error_code& ec)
    {
        std::lock_guard<std::mutex> lock(failureSync);
        if (failureKind.empty())
        {
            failureKind = kind;
            failureMessage = ec.message();
            failurePromise.set_value();
        }
    }

    /// @brief waits for the connection attempt to fail and returns which callback reported it
    std::string awaitFailureKind()
    {
        if (failureFuture.wait_for(timeout) != std::future_status::ready)
            return "(no failure reported)";
        std::lock_guard<std::mutex> lock(failureSync);
        return failureKind;
    }

    std::string failureDescription()
    {
        std::lock_guard<std::mutex> lock(failureSync);
        return failureKind + ": " + failureMessage;
    }

    std::mutex failureSync;
    std::string failureKind;
    std::string failureMessage;
    std::promise<void> failurePromise;
    std::future<void> failureFuture = failurePromise.get_future();
};

TEST_F(TlsConnectionTest, MutualTlsSucceeds)
{
    serverCaFile = test_secrets::CaCert;
    clientCertFile = test_secrets::ClientCert;
    clientKeyFile = test_secrets::ClientKey;

    auto server = createServer();
    auto client = createClient();
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready) << failureDescription();
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);
}

TEST_F(TlsConnectionTest, MutualTlsRejectsClientWithoutCertificate)
{
    serverCaFile = test_secrets::CaCert;

    auto server = createServer();
    auto client = createClient();
    client->connect();

    // under TLS 1.3 the server rejects the certificate after the client's handshake is complete, so
    // this surfaces on the web-socket handshake - the library routes it by the category of the error
    ASSERT_EQ(awaitFailureKind(), "tls") << failureDescription();
    ASSERT_EQ(clientConnectedFuture.wait_for(std::chrono::milliseconds(200)), std::future_status::timeout);
}

TEST_F(TlsConnectionTest, UntrustedServerRejected)
{
    clientCaFile = test_secrets::OtherCaCert;

    auto server = createServer();
    auto client = createClient();
    client->connect();

    ASSERT_EQ(awaitFailureKind(), "tls") << failureDescription();
    ASSERT_EQ(clientConnectedFuture.wait_for(std::chrono::milliseconds(200)), std::future_status::timeout);
}

TEST_F(TlsConnectionTest, EncryptsWithoutVerifyingServer)
{
    clientCaFile.clear();
    verifyServer = false;

    auto server = createServer();
    auto client = createClient();
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready) << failureDescription();
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);
}

TEST_F(TlsConnectionTest, PlaintextClientRejectedOnTlsPort)
{
    auto server = createServer();

    transportMode = TransportMode::Plaintext;
    auto client = createClient();
    client->connect();

    ASSERT_NE(awaitFailureKind(), "(no failure reported)");
    ASSERT_EQ(clientConnectedFuture.wait_for(std::chrono::milliseconds(200)), std::future_status::timeout);
}

TEST_F(TlsConnectionTest, TlsClientRejectedOnPlaintextPort)
{
    transportMode = TransportMode::Plaintext;
    auto server = createServer();

    transportMode = TransportMode::Tls;
    auto client = createClient();
    client->connect();

    ASSERT_NE(awaitFailureKind(), "(no failure reported)");
    ASSERT_EQ(clientConnectedFuture.wait_for(std::chrono::milliseconds(200)), std::future_status::timeout);
}

TEST_F(TlsConnectionTest, BothChannelsAtOnce)
{
    const uint16_t plaintextPort = CONNECTION_PORT;
    const uint16_t tlsPort = CONNECTION_PORT + 1;

    auto server = std::make_shared<Server>(onNewServerSessionCallback, onAuthenticateCallback, ioContextPtrServer, logCallback);
    ASSERT_FALSE(server->start(plaintextPort));
    ASSERT_FALSE(server->startTls(tlsPort, test_secrets::ServerCert, test_secrets::ServerKey));

    transportMode = TransportMode::Plaintext;
    auto plaintextClient = createClient(std::string(), plaintextPort);
    plaintextClient->connect();
    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready) << failureDescription();
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    ResetFuturePromise();

    transportMode = TransportMode::Tls;
    auto tlsClient = createClient(std::string(), tlsPort);
    tlsClient->connect();
    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready) << failureDescription();
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);
}

TEST_F(TlsConnectionTest, DataOverTls)
{
    auto server = createServer();
    auto client = createClient();
    client->connect();

    ASSERT_EQ(clientConnectedFuture.wait_for(timeout), std::future_status::ready) << failureDescription();
    ASSERT_EQ(serverConnectedFuture.wait_for(timeout), std::future_status::ready);

    const std::string payload = "encrypted payload";

    std::promise<std::string> receivedPromise;
    std::future<std::string> receivedFuture = receivedPromise.get_future();
    serverSession->scheduleRead(ReadTask(
        [&receivedPromise](const void* data, size_t size) -> ReadTask
        {
            receivedPromise.set_value(std::string(static_cast<const char*>(data), size));
            return ReadTask();
        },
        payload.size()));

    BatchedWriteTasks tasks;
    tasks.push_back(WriteTask(boost::asio::const_buffer(payload.data(), payload.size()), []() {}));
    clientSession->scheduleWrite(std::move(tasks), std::nullopt);

    ASSERT_EQ(receivedFuture.wait_for(timeout), std::future_status::ready);
    ASSERT_EQ(receivedFuture.get(), payload);
}

TEST_F(TlsConnectionTest, ServerReportsMissingSecrets)
{
    auto server = std::make_shared<Server>(onNewServerSessionCallback, onAuthenticateCallback, ioContextPtrServer, logCallback);

    // a secret which cannot be read is an error the caller is told about, not an exception
    boost::system::error_code ec;
    ASSERT_NO_THROW(ec = server->startTls(CONNECTION_PORT, test_secrets::MissingCert, test_secrets::ServerKey));
    ASSERT_TRUE(ec);
}

TEST_F(TlsConnectionTest, ClientRejectsMissingSecrets)
{
    clientCaFile = test_secrets::MissingCert;
    ASSERT_THROW(createClient(), boost::system::system_error);
}

TEST_F(TlsConnectionTest, ClientRejectsCertificateWithoutKey)
{
    clientCertFile = test_secrets::ClientCert;
    clientKeyFile.clear();
    ASSERT_THROW(createClient(), std::invalid_argument);
}

TEST_F(TlsConnectionTest, ClientRejectsVerificationWithoutAuthority)
{
    clientCaFile.clear();
    ASSERT_THROW(createClient(), std::invalid_argument);
}

#endif
