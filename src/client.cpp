#include <native_streaming/client.hpp>
#include <boost/asio/ip/tcp.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

Client::Client(const std::string& host,
               const std::string& port,
               const std::string& path,
               OnNewSessionCallback onNewSessionCallback,
               OnCompleteCallback onConnectionFailedCallback,
               std::shared_ptr<boost::asio::io_context> ioContextPtr,
               LogCallback logCallback)
    : ioContextPtr(ioContextPtr)
    , logCallback(logCallback)
    , host(host)
    , port(port)
    , path(path)
    , resolver(*ioContextPtr)
    , onNewSessionCallback(onNewSessionCallback)
    , onConnectionFailedCallback(onConnectionFailedCallback)
{
}

Client::~Client()
{
    NS_LOG_I("Shutting down the client");
}

void Client::connect()
{
    NS_LOG_I("connecting to server: host {}, port {}, path {}", host, port, path);

    resolver.async_resolve(
        host,
        port,
        [this, weak_self = weak_from_this()](const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results)
        {
            if (auto shared_self = weak_self.lock())
                onResolve(ec, results);
        });
}

void Client::onResolve(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results)
{
    if (ec)
    {
        NS_LOG_E("resolve failed {}", ec.message());
        onConnectionFailedCallback(ec);
        return;
    }

    auto wsStream = std::make_shared<WebsocketStream>(*ioContextPtr);
    boost::beast::get_lowest_layer(*wsStream).async_connect(
        results,
        [this, weak_self = weak_from_this(), wsStream](const boost::system::error_code& ec,
                                                       boost::asio::ip::tcp::resolver::results_type::endpoint_type)
        {
            if (auto shared_self = weak_self.lock())
                onConnect(ec, wsStream);
        });
}

void Client::onConnect(const boost::system::error_code& ec, std::shared_ptr<WebsocketStream> wsStream)
{
    if (ec)
    {
        NS_LOG_E("connect failed {}", ec.message());
        onConnectionFailedCallback(ec);
        return;
    }

    // Set a decorator to change the User-Agent of the handshake
    wsStream->set_option(boost::beast::websocket::stream_base::decorator(
        [](boost::beast::websocket::request_type& req)
        { req.set(boost::beast::http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " openDAQ-streaming-client"); }));

    wsStream->async_handshake(host,
                              path,
                              [this, weak_self = weak_from_this(), wsStream](const boost::system::error_code& ec)
                              {
                                  if (auto shared_self = weak_self.lock())
                                      onUpgradeConnection(ec, wsStream);
                              });
}

void Client::onUpgradeConnection(const boost::system::error_code& ec, std::shared_ptr<WebsocketStream> wsStream)
{
    if (ec)
    {
        NS_LOG_E("handshake failed {}", ec.message());
        onConnectionFailedCallback(ec);
        return;
    }

    onNewSessionCallback(createSession(wsStream));
}

std::shared_ptr<Session> Client::createSession(std::shared_ptr<WebsocketStream> wsStream)
{
    return std::make_shared<Session>(ioContextPtr, wsStream, boost::beast::role_type::client, logCallback);
}

END_NAMESPACE_NATIVE_STREAMING
