#include <native_streaming/server.hpp>
#include <boost/asio/ip/tcp.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

Server::Server(OnNewSessionCallback onNewSessionCallback, std::shared_ptr<boost::asio::io_context> ioContextPtr, LogCallback logCallback)
    : ioContextPtr(ioContextPtr)
    , logCallback(logCallback)
    , onNewSessionCallback(onNewSessionCallback)
    , tcpAcceptorV4(*ioContextPtr)
{
}

Server::~Server()
{
    stop();
    NS_LOG_T("~Server");
}

boost::system::error_code Server::start(uint16_t port)
{
    NS_LOG_D("Starting server");

    boost::system::error_code ec;

    tcpAcceptorV4.open(boost::asio::ip::tcp::v4(), ec);
    if (ec)
    {
        NS_LOG_E("Server failed to initialize acceptor: {}", ec.message());
        return ec;
    }
    try
    {
        tcpAcceptorV4.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        tcpAcceptorV4.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
        tcpAcceptorV4.listen();
    }
    catch (const boost::system::system_error& e)
    {
        NS_LOG_E("Server failed to initialize tcp V4 acceptor: {}", e.code().message());
        return e.code();
    }

    startTcpAccept(tcpAcceptorV4);

    return boost::system::error_code();
}

void Server::startTcpAccept(boost::asio::ip::tcp::acceptor& tcpAcceptor)
{
    tcpAcceptor.async_accept(
        [this, weak_self = weak_from_this()](const boost::system::error_code& ec, boost::asio::ip::tcp::socket&& socket)
        {
            if (auto shared_self = weak_self.lock())
                onAcceptTcpConnection(ec, std::move(socket));
        });
}

void Server::stopTcpAccept(boost::asio::ip::tcp::acceptor& tcpAcceptor)
{
    if (tcpAcceptor.is_open())
        tcpAcceptor.close();
}

void Server::stop()
{
    NS_LOG_D("stopping server");
    stopTcpAccept(tcpAcceptorV4);
}

void Server::onAcceptTcpConnection(const boost::system::error_code& ec, boost::asio::ip::tcp::socket&& socket)
{
    if (ec)
    {
        if (ec.value() == boost::asio::error::operation_aborted)
        {
            NS_LOG_T("Accept operation cancelled: {}", ec.message());
        }
        else
        {
            NS_LOG_E("accept failed {}", ec.message());
        }
        return;
    }

    NS_LOG_T("server accepting new connection");

    auto wsStream = std::make_shared<WebsocketStream>(std::move(socket));

    // Set a decorator to change the Server-Agent of the handshake
    wsStream->set_option(boost::beast::websocket::stream_base::decorator(
        [](boost::beast::websocket::response_type& res)
        { res.set(boost::beast::http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " openDAQ-streaming-server"); }));

    wsStream->async_accept([this, weak_self = weak_from_this(), wsStream](const boost::system::error_code& ec)
                           {
                               if (auto shared_self = weak_self.lock())
                                    onUpgradeConnection(ec, wsStream);
                           });

    startTcpAccept(tcpAcceptorV4);
}

void Server::onUpgradeConnection(const boost::system::error_code& ec, std::shared_ptr<WebsocketStream> wsStream)
{
    std::string id = wsStream->next_layer().socket().remote_endpoint().address().to_string() + ":" +
                     std::to_string(wsStream->next_layer().socket().remote_endpoint().port());
    if (ec)
    {
        NS_LOG_E("Client {} - websocket connection failed: {}", id, ec.message());
        return;
    }

    NS_LOG_I("Client {} - websocket connection accepted", id);
    onNewSessionCallback(createSession(wsStream));
}

std::shared_ptr<Session> Server::createSession(std::shared_ptr<WebsocketStream> wsStream)
{
    return std::make_shared<Session>(ioContextPtr, wsStream, boost::beast::role_type::server, logCallback);
}

END_NAMESPACE_NATIVE_STREAMING
