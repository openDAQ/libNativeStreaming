#include <native_streaming/server.hpp>
#include <gtest/gtest.h>
#include <native_streaming/logging.hpp>
#include "test_base.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/write.hpp>
#include <array>
#include <future>
#include <ostream>
#include <string>

using namespace daq::native_streaming;

namespace raw_connection
{

enum class RawConnectionResult
{
    /// TCP connection to the server could not be established
    ConnectFailed,
    /// Server sent response bytes
    Responded,
    /// Server closed or reset the connection without sending a response
    DroppedByServer,
    /// Server neither responded nor closed the connection - the client's timeout reached instead
    TimedOut
};

std::ostream& operator<<(std::ostream& os, RawConnectionResult result)
{
    switch (result)
    {
        case RawConnectionResult::ConnectFailed:
            return os << "ConnectFailed";
        case RawConnectionResult::Responded:
            return os << "Responded";
        case RawConnectionResult::DroppedByServer:
            return os << "DroppedByServer";
        case RawConnectionResult::TimedOut:
            return os << "TimedOut";
    }
    return os;
}

/// Pre-upgrade accept phase is exercised with a plain TCP socket so arbitrary bytes can be sent.
/// Async reads are completed by an io_context already running on another thread.
class RawTcpConnection
{
public:
    RawTcpConnection(boost::asio::io_context& ioContext, const boost::asio::ip::tcp::endpoint& endpoint)
        : socket(ioContext)
    {
        socket.connect(endpoint, connectError);
    }

    bool isConnected() const
    {
        return !connectError;
    }

    /// Sends the payload and waits for the server's reaction
    RawConnectionResult exchange(const std::string& payload, std::chrono::milliseconds ioTimeout)
    {
        if (connectError)
            return RawConnectionResult::ConnectFailed;

        boost::system::error_code writeError;
        boost::asio::write(socket, boost::asio::buffer(payload), writeError);
        if (writeError)
            return RawConnectionResult::DroppedByServer;

        return readFromServer(ioTimeout);
    }

    /// Tries read bytes from server response
    RawConnectionResult readFromServer(std::chrono::milliseconds ioTimeout)
    {
        if (connectError)
            return RawConnectionResult::ConnectFailed;

        auto readPromise = std::make_shared<std::promise<std::pair<boost::system::error_code, size_t>>>();
        auto readFuture = readPromise->get_future();
        socket.async_read_some(boost::asio::buffer(responseData),
                               [readPromise](const boost::system::error_code& ec, size_t size)
                               { readPromise->set_value({ec, size}); });

        if (readFuture.wait_for(ioTimeout) != std::future_status::ready)
        {
            // close on the io thread to cancel the pending read, then let its handler complete before returning
            boost::asio::post(socket.get_executor(),
                              [this]()
                              {
                                  boost::system::error_code ignored;
                                  socket.close(ignored);
                              });
            readFuture.wait();
            return RawConnectionResult::TimedOut;
        }

        const auto [readError, bytesRead] = readFuture.get();
        if (readError || bytesRead == 0)
            return RawConnectionResult::DroppedByServer;

        const std::string response(responseData.data(), bytesRead);
        firstResponseLine = response.substr(0, response.find("\r\n"));
        return RawConnectionResult::Responded;
    }

    std::string firstResponseLine;

private:
    boost::asio::ip::tcp::socket socket;
    boost::system::error_code connectError;
    std::array<char, 4096> responseData{};
};

} // namespace raw_connection

using namespace raw_connection;

class ServerAcceptTest : public TestBase
{
public:
    /// Drops created session
    const OnNewSessionCallback onUnusedSessionCallback = [](std::shared_ptr<Session>) {};

    /// Everyone is authenticated by default
    const OnAuthenticateCallback onAuthenticateCallback = [](const Authentication& authentication, std::shared_ptr<void>& userContextOut)
    { return true; };

    void SetUp() override
    {
        server = std::make_shared<Server>(onUnusedSessionCallback, onAuthenticateCallback, ioContextPtrServer, logCallback);
        ASSERT_EQ(server->start(CONNECTION_PORT), boost::system::error_code());
    }

    void TearDown() override
    {
        server->stop();
    }

    std::string upgradeToWebsocketRequest() const
    {
        return "GET / HTTP/1.1\r\n"
               "Host: " + CONNECTION_HOST + "\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
               "Sec-WebSocket-Version: 13\r\n"
               "\r\n";
    }

    /// Bytes which are not a valid HTTP request; the parser consumes none of them and fails
    std::string malformedRequest() const
    {
        using namespace std::string_literals;
        return "\x00\x01\x02not-http garbage\r\n\r\n"s;
    }

    boost::asio::ip::tcp::endpoint serverEndpoint() const
    {
        return {boost::asio::ip::make_address(CONNECTION_HOST), CONNECTION_PORT};
    }

    const std::chrono::milliseconds TEST_TIMEOUT = std::chrono::milliseconds(2000);

    /// Status line of the server's response to a successful websocket upgrade request
    const std::string UPGRADE_RESPONSE_STATUS_LINE = "HTTP/1.1 101 Switching Protocols";

    std::shared_ptr<Server> server;
};

// A client sending a malformed non-HTTP request must not affect the upgrade of the connections following it
TEST_F(ServerAcceptTest, MalformedRequestDoesNotPoisonLaterConnections)
{
    {
        RawTcpConnection valid(*ioContextPtrClient, serverEndpoint());
        ASSERT_TRUE(valid.isConnected());
        ASSERT_EQ(valid.exchange(upgradeToWebsocketRequest(), TEST_TIMEOUT), RawConnectionResult::Responded);
        ASSERT_EQ(valid.firstResponseLine, UPGRADE_RESPONSE_STATUS_LINE);
    }

    {
        RawTcpConnection poison(*ioContextPtrClient, serverEndpoint());
        ASSERT_TRUE(poison.isConnected());
        ASSERT_EQ(poison.exchange(malformedRequest(), TEST_TIMEOUT), RawConnectionResult::DroppedByServer);
    }

    {
        RawTcpConnection retest(*ioContextPtrClient, serverEndpoint());
        ASSERT_TRUE(retest.isConnected());
        ASSERT_EQ(retest.exchange(upgradeToWebsocketRequest(), TEST_TIMEOUT), RawConnectionResult::Responded);
        ASSERT_EQ(retest.firstResponseLine, UPGRADE_RESPONSE_STATUS_LINE);
    }
}

// A client connecting without sending a request must not block the upgrade of other connections
TEST_F(ServerAcceptTest, DISABLED_SilentClientDoesNotBlockOtherConnections)
{
    RawTcpConnection silent(*ioContextPtrClient, serverEndpoint());
    ASSERT_TRUE(silent.isConnected());

    RawTcpConnection other(*ioContextPtrClient, serverEndpoint());
    ASSERT_TRUE(other.isConnected());
    ASSERT_EQ(other.exchange(upgradeToWebsocketRequest(), TEST_TIMEOUT), RawConnectionResult::Responded);
    ASSERT_EQ(other.firstResponseLine, UPGRADE_RESPONSE_STATUS_LINE);
}

// A client connecting without sending a request must be disconnected by the server eventually
TEST_F(ServerAcceptTest, DISABLED_SilentClientIsDisconnected)
{
    RawTcpConnection silent(*ioContextPtrClient, serverEndpoint());
    ASSERT_TRUE(silent.isConnected());
    ASSERT_EQ(silent.readFromServer(std::chrono::seconds(10)), RawConnectionResult::DroppedByServer);
}
