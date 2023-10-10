#include <native_streaming/session.hpp>
#include <boost/asio/read.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

Session::Session(std::shared_ptr<boost::asio::io_context> ioContextPtr,
                 std::shared_ptr<WebsocketStream> wsStream,
                 boost::beast::role_type role,
                 LogCallback logCallback)
    : role(role)
    , logCallback(logCallback)
    , ioContextPtr(ioContextPtr)
    , reader(std::make_shared<AsyncReader>(*ioContextPtr, wsStream, logCallback))
    , writer(std::make_shared<AsyncWriter>(*ioContextPtr, wsStream, logCallback))
    , wsStream(wsStream)
{
    setOptions();
}

Session::~Session()
{
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
    NS_LOG_T("Closing {} session", (role == boost::beast::role_type::server) ? "server" : "client");

    wsStream->async_close(boost::beast::websocket::close_code::normal,
                          [this, onClosedCallback, weak_self = weak_from_this()](const boost::system::error_code& ec)
                          {
                              if (auto shared_self = weak_self.lock())
                              {
                                  std::string roleName =
                                      (role == boost::beast::role_type::server) ? "server" : "client";
                                  if (wsStream->is_open())
                                  {
                                      NS_LOG_W("Closing {} session failure: {}", roleName, ec.message());
                                      onClosedCallback(ec);
                                  }
                                  else
                                  {
                                      NS_LOG_T("Closed {} session", roleName);
                                      onClosedCallback(boost::system::error_code());
                                  }
                              }
                          });
}

void Session::setErrorHandlers(OnCompleteCallback onWriteErrorCallback, OnCompleteCallback onReadErrorCallback)
{
    writer->setErrorHandler(onWriteErrorCallback);
    reader->setErrorHandler(onReadErrorCallback);
}

void Session::scheduleRead(const ReadTask& entryTask)
{
    reader->scheduleRead(entryTask);
}

void Session::scheduleWrite(const std::vector<WriteTask>& tasks)
{
    writer->scheduleWrite(tasks);
}

END_NAMESPACE_NATIVE_STREAMING
