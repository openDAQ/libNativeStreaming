#include <native_streaming/session.hpp>
#include <boost/asio/read.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

static std::chrono::milliseconds defaultHeartbeatPeriod = std::chrono::milliseconds(1000);

Session::Session(std::shared_ptr<boost::asio::io_context> ioContextPtr,
                 std::shared_ptr<WebsocketStream> wsStream,
                 std::shared_ptr<void> userContext,
                 boost::beast::role_type role,
                 LogCallback logCallback,
                 const std::string& endpointAddress,
                 const boost::asio::ip::port_type& endpointPortNumber)
    : role(role)
    , logCallback(logCallback)
    , ioContextPtr(ioContextPtr)
    , reader(std::make_shared<AsyncReader>(*ioContextPtr, wsStream, logCallback))
    , writer(std::make_shared<AsyncWriter>(*ioContextPtr, wsStream, logCallback))
    , wsStream(wsStream)
    , userContext(userContext)
    , heartbeatTimer(std::make_shared<boost::asio::steady_timer>(*ioContextPtr.get()))
    , heartbeatPeriod(defaultHeartbeatPeriod)
    , endpointAddress(endpointAddress)
    , endpointPortNumber(endpointPortNumber)
{
    setOptions();
}

Session::~Session()
{
    heartbeatTimer->cancel();
    // cancel all async operations on socket
    wsStream->next_layer().cancel();
}

void Session::setOptions()
{
    using namespace std::chrono_literals;

    // websocket stream handles timeouts on its own - timeout on tcp stream should be turned off
    boost::beast::get_lowest_layer(*wsStream).expires_never();
    wsStream->binary(true);

    // Set reduced timeout settings for the websocket
    auto option = boost::beast::websocket::stream_base::timeout::suggested(role);
    option.handshake_timeout = 3s;
    wsStream->set_option(option);
}

void Session::close(OnCompleteCallback onClosedCallback)
{
    NS_LOG_D("Disconnection: closing {}-side native communication session", (role == boost::beast::role_type::server) ? "server" : "client");

    wsStream->async_close(boost::beast::websocket::close_code::normal,
                          [this, onClosedCallback, weak_self = weak_from_this()](const boost::system::error_code& ec)
                          {
                              if (auto shared_self = weak_self.lock())
                              {
                                  std::string roleName =
                                      (role == boost::beast::role_type::server) ? "server" : "client";
                                  if (wsStream->is_open())
                                  {
                                      NS_LOG_E("Disconnected with closing {}-side session failure: {}", roleName, ec.message());
                                      onClosedCallback(ec);
                                  }
                                  else
                                  {
                                      NS_LOG_D("Disconnected with {}-side session normally closed.", roleName);
                                      onClosedCallback(boost::system::error_code());
                                  }
                              }
                              else
                              {
                                  onClosedCallback(boost::system::error_code());
                              }
                          });
}

void Session::setErrorHandlers(OnSessionErrorCallback onWriteErrorCallback,
                               OnSessionErrorCallback onReadErrorCallback)
{
    writer->setErrorHandler(
        [onWriteErrorCallback, weak_self = weak_from_this()](const boost::system::error_code& ec)
        {
            if (auto shared_self = weak_self.lock())
            {
                onWriteErrorCallback(ec.message(), shared_self);
            }
        });
    reader->setErrorHandler(
        [onReadErrorCallback, weak_self = weak_from_this()](const boost::system::error_code& ec)
        {
            if (auto shared_self = weak_self.lock())
            {
                onReadErrorCallback(ec.message(), shared_self);
            }
        });
}

void Session::scheduleRead(const ReadTask& entryTask)
{
    reader->scheduleRead(entryTask);
}

void Session::scheduleWrite(BatchedWriteTasks&& tasks, OptionalWriteDeadline&& deadlineTime)
{
    writer->scheduleWrite(std::move(tasks), std::move(deadlineTime));
}

void Session::restartHeartbeatTimer()
{
    heartbeatTimer->expires_from_now(heartbeatPeriod);
    heartbeatTimer->async_wait(
        [this, weak_self = weak_from_this()](const boost::system::error_code& ec)
        {
            if (ec)
                return;
            if (auto shared_self = weak_self.lock())
            {
                this->schedulePong();
            }
        });
}

void Session::schedulePong()
{
    if (!wsStream->is_open())
        return;

    std::string payload = std::string("ping from ") +
                          std::string((role == boost::beast::role_type::server) ? "server" : "client");
    wsStream->async_pong(payload.c_str(),
                         [this, weak_self = weak_from_this()](const boost::system::error_code& ec)
                         {
                             if (ec)
                                 return;
                             if (auto shared_self = weak_self.lock())
                             {
                                 this->restartHeartbeatTimer();
                             }
                         });
}

void Session::startConnectionActivityMonitoring(OnConnectionAliveCallback connectionAliveCallback, std::chrono::milliseconds heartbeatPeriod)
{
    this->connectionAliveCallback = connectionAliveCallback;
    this->heartbeatPeriod = heartbeatPeriod;
    reader->setConnectionAliveHandler(connectionAliveCallback);
    // do not treat a successful write as an indicator of alive connection as it can succeed
    // even if the receiver is unreachable when intermediate network nodes present, such as a routers
    //writer->setConnectionAliveHandler(connectionAliveCallback);

    wsStream->control_callback(
        [this, weak_self = weak_from_this()](boost::beast::websocket::frame_type kind, boost::beast::string_view /*payload*/)
        {
            if (auto shared_self = weak_self.lock())
            {
                if (kind == boost::beast::websocket::frame_type::pong)
                {
                    this->connectionAliveCallback();
                }
            }
        });
    schedulePong();
}

bool Session::isOpen()
{
    return wsStream->is_open();
}

std::shared_ptr<void> Session::getUserContext()
{
    return userContext;
}

std::string Session::getEndpointAddress()
{
    return endpointAddress;
}

boost::asio::ip::port_type Session::getEndpointPortNumber()
{
    return endpointPortNumber;
}

void Session::setWriteTimedOutHandler(OnSessionErrorCallback writeTaskTimeoutHandler)
{
    writer->setWriteTimedOutHandler(
        [writeTaskTimeoutHandler, weak_self = weak_from_this()]()
        {
            if (auto shared_self = weak_self.lock())
            {
                writeTaskTimeoutHandler("Write task timed out", shared_self);
            }
        });
}

END_NAMESPACE_NATIVE_STREAMING
