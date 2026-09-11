#include <native_streaming/server.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <native_streaming/utils/boost_compatibility_utils.hpp>

#include <algorithm>
#include <chrono>
#include <type_traits>

#if NATIVE_STREAMING_ENABLE_TLS
#include <boost/asio/ssl/stream_base.hpp>
#endif

BEGIN_NAMESPACE_NATIVE_STREAMING

namespace
{

#if NATIVE_STREAMING_ENABLE_TLS
/// @brief how long the server waits for the client of a rejected connection to close it
constexpr auto rejectedConnectionDrainTimeout = std::chrono::seconds(1);
#endif

/// @brief applies the buffer limits the native transport protocol needs, whatever the stream runs on
template <typename Stream>
void applyStreamLimits(Stream& wsStream)
{
    wsStream.write_buffer_bytes(65536);
    // 256 MB - one byte bigger than max payload size within native transport protocol used above websocket connection
    wsStream.read_message_max(0x10000000);
}

}

Server::Server(OnNewSessionCallback onNewSessionCallback,
               OnAuthenticateCallback onAuthenticateCallback,
               std::shared_ptr<boost::asio::io_context> ioContextPtr,
               LogCallback logCallback)
    : ioContextPtr(ioContextPtr)
    , logCallback(logCallback)
    , onNewSessionCallback(onNewSessionCallback)
    , onAuthenticateCallback(onAuthenticateCallback)
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

    auto listener = std::make_shared<Listener>(*ioContextPtr);
    const auto ec = startListener(listener, port);
    if (!ec)
        listeners.push_back(std::move(listener));

    return ec;
}

#if NATIVE_STREAMING_ENABLE_TLS

boost::system::error_code Server::startTls(uint16_t port,
                                           const std::string& certFile,
                                           const std::string& keyFile,
                                           const std::string& caFile)
{
    NS_LOG_D("Starting TLS server");

    std::unique_ptr<boost::asio::ssl::context> sslContext;
    try
    {
        sslContext = std::make_unique<boost::asio::ssl::context>(makeServerTlsContext(certFile, keyFile, caFile));
    }
    catch (const boost::system::system_error& e)
    {
        NS_LOG_E("Server failed to load the TLS secrets: {}", e.code().message());
        return e.code();
    }
    catch (const std::exception& e)
    {
        NS_LOG_E("Server failed to configure TLS: {}", e.what());
        return boost::asio::error::invalid_argument;
    }

    if (caFile.empty())
    {
        NS_LOG_I("TLS listener does not request client certificates");
    }
    else
    {
        NS_LOG_I("TLS listener requires client certificates signed by {}", caFile);
    }

    auto listener = std::make_shared<Listener>(*ioContextPtr);
    listener->sslContext = std::move(sslContext);

    const auto ec = startListener(listener, port);
    if (!ec)
        listeners.push_back(std::move(listener));

    return ec;
}

#endif

boost::system::error_code Server::startListener(const std::shared_ptr<Listener>& listener, uint16_t port)
{
    const auto listensOnPort = [port](const std::shared_ptr<Listener>& other) { return other->port == port; };
    if (port != 0 && std::any_of(listeners.begin(), listeners.end(), listensOnPort))
    {
        NS_LOG_E("Server already listens on port {}", port);
        return boost::asio::error::address_in_use;
    }
    listener->port = port;

    boost::system::error_code ec;
    bool hasTcpAcceptor = false;

    listener->tcpAcceptorV4.open(boost::asio::ip::tcp::v4(), ec);
    if (!ec)
    {
        try
        {
            listener->tcpAcceptorV4.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            listener->tcpAcceptorV4.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
            listener->tcpAcceptorV4.listen();
            startTcpAccept(listener, listener->tcpAcceptorV4);
            hasTcpAcceptor = true;
        }
        catch (const boost::system::system_error& e)
        {
            NS_LOG_W("Server failed to initialize tcp V4 acceptor: {}", e.code().message());
            ec = e.code();
            listener->tcpAcceptorV4.close();
        }
    }
    else
    {
        NS_LOG_W("Server failed to open tcp V4 acceptor: {}", ec.message());
    }

    listener->tcpAcceptorV6.open(boost::asio::ip::tcp::v6(), ec);
    if (!ec)
    {
        try
        {
            listener->tcpAcceptorV6.set_option(boost::asio::ip::v6_only(true));
            listener->tcpAcceptorV6.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            listener->tcpAcceptorV6.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), port));
            listener->tcpAcceptorV6.listen();
            startTcpAccept(listener, listener->tcpAcceptorV6);
            hasTcpAcceptor = true;
        }
        catch (const boost::system::system_error& e)
        {
            NS_LOG_W("Server failed to initialize tcp V6 acceptor: {}", e.code().message());
            ec = e.code();
            listener->tcpAcceptorV6.close();
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

void Server::startTcpAccept(const std::shared_ptr<Listener>& listener, boost::asio::ip::tcp::acceptor& tcpAcceptor)
{
    if (!tcpAcceptor.is_open())
        return;

    // the listener is captured by shared pointer; the acceptor referenced alongside it stays
    // alive for as long as this operation does, even if the server stops meanwhile
    tcpAcceptor.async_accept(
        [this, weak_self = weak_from_this(), listener, &tcpAcceptor](const boost::system::error_code& ec,
                                                                     boost::asio::ip::tcp::socket&& socket)
        {
            if (auto shared_self = weak_self.lock())
                onAcceptTcpConnection(listener, tcpAcceptor, ec, std::move(socket));
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

    for (const auto& listener : listeners)
    {
        stopTcpAccept(listener->tcpAcceptorV4);
        stopTcpAccept(listener->tcpAcceptorV6);
    }

    listeners.clear();
}

void Server::onAcceptTcpConnection(const std::shared_ptr<Listener>& listener,
                                   boost::asio::ip::tcp::acceptor& tcpAcceptor,
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
            startTcpAccept(listener, tcpAcceptor);
        }

        return;
    }

    NS_LOG_T("server accepting new connection");

#if NATIVE_STREAMING_ENABLE_TLS
    if (listener->sslContext)
    {
        auto wsStream = std::make_shared<TlsWebsocketStream>(std::move(socket), *listener->sslContext);
        applyStreamLimits(*wsStream);
        startTlsHandshake(listener, tcpAcceptor, std::make_shared<AcceptOp<TlsWebsocketStream>>(wsStream));
        return;
    }
#endif

    auto wsStream = std::make_shared<WebsocketStream>(std::move(socket));
    applyStreamLimits(*wsStream);
    startReadAcceptRequest(listener, tcpAcceptor, std::make_shared<AcceptOp<WebsocketStream>>(wsStream));
}

#if NATIVE_STREAMING_ENABLE_TLS

template <typename Stream>
void Server::startTlsHandshake(const std::shared_ptr<Listener>& listener,
                               boost::asio::ip::tcp::acceptor& tcpAcceptor,
                               std::shared_ptr<AcceptOp<Stream>> acceptOp)
{
    acceptOp->wsStream->next_layer().async_handshake(
        boost::asio::ssl::stream_base::server,
        [this, weak_self = weak_from_this(), listener, &tcpAcceptor, acceptOp](const boost::system::error_code& ec)
        {
            auto shared_self = weak_self.lock();
            if (!shared_self)
                return;

            if (ec)
            {
                NS_LOG_W("TLS handshake with a client failed: {}", ec.message());
                startTcpAccept(listener, tcpAcceptor);
                closeRejectedConnection(acceptOp);
                return;
            }

            startReadAcceptRequest(listener, tcpAcceptor, acceptOp);
        });
}

template <typename Stream>
void Server::closeRejectedConnection(std::shared_ptr<AcceptOp<Stream>> acceptOp)
{
    // the client may already have sent data the server will never read - under TLS 1.3 its web-socket
    // request follows its handshake at once. Closing the socket with unread data makes TCP reset the
    // connection, and on Windows a reset discards what the client has not read yet, the TLS alert
    // telling it why it was rejected included. The alert is already sent by the time the handshake
    // fails, so only the sending side is shut down, and the socket is closed once the client closes
    auto& tcpStream = boost::beast::get_lowest_layer(*acceptOp->wsStream);
    boost::system::error_code ec;
    tcpStream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
    tcpStream.expires_after(rejectedConnectionDrainTimeout);
    drainRejectedConnection(acceptOp);
}

template <typename Stream>
void Server::drainRejectedConnection(std::shared_ptr<AcceptOp<Stream>> acceptOp)
{
    boost::beast::get_lowest_layer(*acceptOp->wsStream)
        .async_read_some(acceptOp->buffer.prepare(1024),
                         [acceptOp](const boost::system::error_code& ec, size_t /*size*/)
                         {
                             // on end of stream, reset or timeout the socket is closed as acceptOp,
                             // which owns it, is released
                             if (!ec)
                                 drainRejectedConnection(acceptOp);
                         });
}

#endif

template <typename Stream>
void Server::startReadAcceptRequest(const std::shared_ptr<Listener>& listener,
                                    boost::asio::ip::tcp::acceptor& tcpAcceptor,
                                    std::shared_ptr<AcceptOp<Stream>> acceptOp)
{
    boost::beast::http::async_read(acceptOp->wsStream->next_layer(),
                                   acceptOp->buffer,
                                   acceptOp->request,
                                   [this, weak_self = weak_from_this(), listener, &tcpAcceptor, acceptOp](
                                       const boost::system::error_code& ec, size_t /*size*/)
                                   {
                                       if (auto shared_self = weak_self.lock())
                                       {
                                           onReadAcceptRequest(ec, acceptOp);
                                           startTcpAccept(listener, tcpAcceptor);
                                       }
                                   });
}

template <typename Stream>
void Server::onReadAcceptRequest(const boost::system::error_code& ec, const std::shared_ptr<AcceptOp<Stream>>& acceptOp)
{
    if (ec)
    {
        NS_LOG_E("Failed to read connect request headers {}", ec.message());
        return;
    }

    const auto& wsStream = acceptOp->wsStream;
    auto& request = acceptOp->request;

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
                                            [this, weak_self = weak_from_this(), acceptOp, userContext](const boost::system::error_code& ecc)
                                            {
                                                if (auto shared_self = weak_self.lock())
                                                    onUpgradeConnection(ecc, acceptOp->wsStream, userContext);
                                            });
}

template <typename Stream>
void Server::onUpgradeConnection(const boost::system::error_code& ec,
                                 std::shared_ptr<Stream> wsStream,
                                 const std::shared_ptr<void>& userContext)
{
    if (ec)
    {
        NS_LOG_E("Connection failed to upgrade to websocket: {}", ec.message());
        return;
    }

    // Pausing the server app with a debugger causes incoming re-/connection attempts to be rejected on the client side,
    // but they remain queued on the server side. When the server resumes, it processes these connections,
    // inevitably failing due to the sockets being in an invalid state. Although the socket appears open,
    // it throws an exception when attempting to retrieve the endpoint address.
    // To handle this, first verify the socket state and then safely attempt to retrieve the endpoint name.
    std::string endpointAddress;
    uint16_t endpointPortNumber;
    if (!(wsStream->is_open() && boost::beast::get_lowest_layer(*wsStream).socket().is_open()))
    {
        NS_LOG_W("Websocket connection aborted: the socket is already closed");
        return;
    }
    else
    {
        try
        {
            auto remoteEp = boost::beast::get_lowest_layer(*wsStream).socket().remote_endpoint();
            endpointAddress = remoteEp.address().to_string();
            endpointPortNumber = remoteEp.port();
        }
        catch (const std::exception& e)
        {
            NS_LOG_W("Websocket connection aborted - cannot get connection endpoint: {}", e.what());
            return;
        }
    }

    NS_LOG_I("Client {} - websocket connection accepted", endpointAddress);

    NS_LOG_D("Websocket connection: auto-fragment {}, max read masg {}, write buffer size {}",
             wsStream->auto_fragment(),
             wsStream->read_message_max(),
             wsStream->write_buffer_bytes());

    onNewSessionCallback(createSession(wsStream, userContext, endpointAddress, endpointPortNumber));
}

template <typename Stream>
std::shared_ptr<Session> Server::createSession(std::shared_ptr<Stream> wsStream,
                                               const std::shared_ptr<void>& userContext,
                                               const std::string& endpointAddress,
                                               const uint16_t& endpointPortNumber)
{
    std::shared_ptr<IWsStream> stream;

    if constexpr (std::is_same_v<Stream, WebsocketStream>)
    {
        stream = makePlainWsStream(wsStream);
    }
#if NATIVE_STREAMING_ENABLE_TLS
    else
    {
        stream = makeTlsWsStream(wsStream);
    }
#endif

    return std::make_shared<Session>(ioContextPtr,
                                     stream,
                                     userContext,
                                     boost::beast::role_type::server,
                                     logCallback,
                                     endpointAddress,
                                     endpointPortNumber);
}

END_NAMESPACE_NATIVE_STREAMING
