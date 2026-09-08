/*
 * Copyright 2022-2026 openDAQ d.o.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <native_streaming/tls.hpp>

#if NATIVE_STREAMING_ENABLE_TLS

#include "ws_stream_impl.hpp"

#include <boost/asio/ssl/error.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

namespace
{

/// @brief the transport-neutral web-socket interface over a TLS-encrypted stream. Every operation is
/// the one the template already implements; only closing has a step of its own.
class TlsWsStream final : public WsStreamImpl<TlsWebsocketStream>
{
public:
    using WsStreamImpl<TlsWebsocketStream>::WsStreamImpl;

    /// @brief performs the web-socket closing handshake and then shuts the TLS session down.
    /// A peer which closes the transport without sending its own close_notify leaves the shutdown
    /// reporting stream_truncated - the common case rather than an error, since the web-socket
    /// closing handshake has already said goodbye at the protocol level - so it is not passed on.
    void asyncClose(boost::beast::websocket::close_code code, OnCompleteCallback onClosedCallback) override
    {
        auto stream = wsStream;
        stream->async_close(
            code,
            [stream, onClosedCallback](const boost::system::error_code& ec)
            {
                if (ec)
                {
                    onClosedCallback(ec);
                    return;
                }

                stream->next_layer().async_shutdown(
                    [onClosedCallback](const boost::system::error_code& shutdownEc)
                    {
                        if (shutdownEc == boost::asio::ssl::error::stream_truncated ||
                            shutdownEc == boost::asio::error::eof)
                            onClosedCallback(boost::system::error_code());
                        else
                            onClosedCallback(shutdownEc);
                    });
            });
    }
};

}

std::shared_ptr<IWsStream> makeTlsWsStream(std::shared_ptr<TlsWebsocketStream> wsStream)
{
    return std::make_shared<TlsWsStream>(std::move(wsStream));
}

END_NAMESPACE_NATIVE_STREAMING

#endif // NATIVE_STREAMING_ENABLE_TLS
