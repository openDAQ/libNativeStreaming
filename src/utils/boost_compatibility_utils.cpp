#include "native_streaming/utils/boost_compatibility_utils.hpp"
#include <string>
#include <functional>
#include <boost/beast/websocket/stream.hpp>

BEGIN_NAMESPACE_STREAM_UTILS

namespace boost_compatibility_utils
{
    void async_handshake(WebsocketStream& stream,
                         const std::string& host,
                         const std::string& target,
                         const BoostHandler& handler)
    {
        stream.async_handshake(host, target, handler);
    }

    void async_accept(WebsocketStream& websocket, const BoostHandler& handler)
    {
        websocket.async_accept(handler);
    }

    void async_accept(WebsocketStream& websocket,
                      boost::beast::http::request<boost::beast::http::string_body>& request,
                      const BoostHandler& handler)
    {
        websocket.async_accept(request, handler);
    }

    void async_write(boost::beast::tcp_stream& stream,
        boost::beast::http::request<boost::beast::http::string_body>& request, WriteCallback callback)
    {
        boost::beast::http::async_write(stream, request, callback);
    }
}

END_NAMESPACE_STREAM_UTILS
