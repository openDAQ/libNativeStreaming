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

#pragma once

#include <native_streaming/ws_stream.hpp>
#include <boost/asio/read.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

/// @brief implements the transport-neutral web-socket interface over a concrete beast web-socket
/// stream. Instantiated once per transport - the operations the library uses are spelled the same
/// way whether the web-socket runs on a plaintext or on an encrypted stream.
template <typename Stream>
class WsStreamImpl : public IWsStream
{
public:
    explicit WsStreamImpl(std::shared_ptr<Stream> wsStream)
        : wsStream(std::move(wsStream))
    {
    }

    void configure(boost::beast::role_type role) override
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

    void asyncReadAtLeast(boost::asio::streambuf& buffer,
                          std::size_t bytesToRead,
                          OnRWCallback onReadCallback) override
    {
        boost::asio::async_read(*wsStream,
                                buffer,
                                boost::asio::transfer_at_least(bytesToRead),
                                std::move(onReadCallback));
    }

    void asyncWrite(const const_buffer_small_vector& buffers, OnRWCallback onWriteCallback) override
    {
        wsStream->async_write(buffers, std::move(onWriteCallback));
    }

    void asyncClose(boost::beast::websocket::close_code code, OnCompleteCallback onClosedCallback) override
    {
        wsStream->async_close(code, std::move(onClosedCallback));
    }

    void asyncPong(const std::string& payload, OnCompleteCallback onSentCallback) override
    {
        wsStream->async_pong(payload.c_str(), std::move(onSentCallback));
    }

    void setControlCallback(OnControlCallback controlCallback) override
    {
        wsStream->control_callback(std::move(controlCallback));
    }

    bool isOpen() const override
    {
        return wsStream->is_open();
    }

    void cancelLowestLayer() override
    {
        boost::beast::get_lowest_layer(*wsStream).cancel();
    }

protected:
    std::shared_ptr<Stream> wsStream;
};

END_NAMESPACE_NATIVE_STREAMING
