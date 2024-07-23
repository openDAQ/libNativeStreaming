#include <native_streaming/server.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <native_streaming/utils/boost_compatibility_utils.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

Server::Server(OnNewSessionCallback onNewSessionCallback,
               OnAuthenticateCallback onAuthenticateCallback,
               std::shared_ptr<boost::asio::io_context> ioContextPtr,
               LogCallback logCallback)
    : ioContextPtr(ioContextPtr)
    , logCallback(logCallback)
    , onNewSessionCallback(onNewSessionCallback)
    , onAuthenticateCallback(onAuthenticateCallback)
    , tcpAcceptorV4(*ioContextPtr)
    , tcpAcceptorV6(*ioContextPtr)
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
    bool hasTcpAcceptor = false;

    tcpAcceptorV4.open(boost::asio::ip::tcp::v4(), ec);
    if (!ec)
    {
        try
        {
            tcpAcceptorV4.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            tcpAcceptorV4.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
            tcpAcceptorV4.listen();
            startTcpAccept(tcpAcceptorV4);
            hasTcpAcceptor = true;
        }
        catch (const boost::system::system_error& e)
        {
            NS_LOG_W("Server failed to initialize tcp V4 acceptor: {}", e.code().message());
            ec = e.code();
            tcpAcceptorV4.close();
        }
    }
    else
    {
        NS_LOG_W("Server failed to open tcp V4 acceptor: {}", ec.message());
    }

    tcpAcceptorV6.open(boost::asio::ip::tcp::v6(), ec);
    if (!ec)
    {
        try
        {
            tcpAcceptorV6.set_option(boost::asio::ip::v6_only(true));
            tcpAcceptorV6.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            tcpAcceptorV6.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), port));
            tcpAcceptorV6.listen();
            startTcpAccept(tcpAcceptorV6);
            hasTcpAcceptor = true;
        }
        catch (const boost::system::system_error& e)
        {
            NS_LOG_W("Server failed to initialize tcp V6 acceptor: {}", e.code().message());
            ec = e.code();
            tcpAcceptorV6.close();
        }
    }
    else
    {
        NS_LOG_W("Server failed to open tcp V6 acceptor: {}", ec.message());
    }

    if (!hasTcpAcceptor)
    {
        NS_LOG_E("Server failed to initialize any tcp acceptor. Last error: {}", ec.message());
        return ec;
    }

    return boost::system::error_code();
}

void Server::startTcpAccept(boost::asio::ip::tcp::acceptor& tcpAcceptor)
{
    if (!tcpAcceptor.is_open())
        return;

    tcpAcceptor.async_accept(
        [this, weak_self = weak_from_this(), &tcpAcceptor](const boost::system::error_code& ec, boost::asio::ip::tcp::socket&& socket)
        {
            if (auto shared_self = weak_self.lock())
                onAcceptTcpConnection(tcpAcceptor, ec, std::move(socket));
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
    stopTcpAccept(tcpAcceptorV6);
}

void Server::onAcceptTcpConnection(boost::asio::ip::tcp::acceptor& tcpAcceptor,
                                   const boost::system::error_code& ec,
                                   boost::asio::ip::tcp::socket&& socket)
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
            startTcpAccept(tcpAcceptor);
        }

        return;
    }

    NS_LOG_T("server accepting new connection");

    auto wsStream = std::make_shared<WebsocketStream>(std::move(socket));
    wsStream->write_buffer_bytes(65536);

    boost::beast::http::async_read(wsStream->next_layer(),
                                   acceptBuffer,
                                   acceptRequest,
                                   [this, weak_self = weak_from_this(), &tcpAcceptor, wsStream](const boost::system::error_code& ec, size_t /*size*/)
                                   {
                                       if (auto sharedSelf = weak_self.lock())
                                       {
                                           onReadAcceptRequest(ec, wsStream, acceptRequest);
                                           startTcpAccept(tcpAcceptor);
                                       }
                                   });
}

void Server::onReadAcceptRequest(const boost::system::error_code& ec,
                                 const std::shared_ptr<WebsocketStream>& wsStream,
                                 boost::beast::http::request<boost::beast::http::string_body>& request)
{
    if (ec)
    {
        NS_LOG_E("Failed to read connect request headers {}", ec.message());
        return;
    }

    auto authentication = Authentication();

    try
    {
        if (request.count(boost::beast::http::field::authorization))
        {
            const auto authorizationHeader = request[boost::beast::http::field::authorization];
            authentication = Authentication::fromHeader(std::string(authorizationHeader));
        }
    }
    catch (...)
    {
        NS_LOG_W("Server recieved invalid authenitcation information");
    }

    std::shared_ptr<void> userContext;
    if (!onAuthenticateCallback(authentication, userContext))
    {
        NS_LOG_I("Websocket authenticaiton failed");
        return;
    }

    // Set a decorator to change the Server-Agent of the handshake
    wsStream->set_option(boost::beast::websocket::stream_base::decorator(
        [](boost::beast::websocket::response_type& res)
        { res.set(boost::beast::http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " openDAQ-streaming-server"); }));

    // Accept the upgrade request
    boost_compatibility_utils::async_accept(*wsStream,
                                            request,
                                            [this, weak_self = weak_from_this(), wsStream, userContext](const boost::system::error_code& ecc)
                                            {
                                                if (auto shared_self = weak_self.lock())
                                                    onUpgradeConnection(ecc, wsStream, userContext);
                                            });
}

void Server::onUpgradeConnection(const boost::system::error_code& ec,
                                 std::shared_ptr<WebsocketStream> wsStream,
                                 const std::shared_ptr<void>& userContext)
{
    std::string id = wsStream->next_layer().socket().remote_endpoint().address().to_string() + ":" +
                     std::to_string(wsStream->next_layer().socket().remote_endpoint().port());
    if (ec)
    {
        NS_LOG_E("Client {} - websocket connection failed: {}", id, ec.message());
        return;
    }

    NS_LOG_I("Client {} - websocket connection accepted", id);
    onNewSessionCallback(createSession(wsStream, userContext));
}

std::shared_ptr<Session> Server::createSession(std::shared_ptr<WebsocketStream> wsStream, const std::shared_ptr<void>& userContext)
{
    return std::make_shared<Session>(ioContextPtr, wsStream, userContext, boost::beast::role_type::server, logCallback);
}

END_NAMESPACE_NATIVE_STREAMING
