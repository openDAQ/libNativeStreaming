/*
 * Copyright 2022-2023 Blueberry d.o.o.
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
#include <string>
#include <functional>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/stream.hpp>

#define BEGIN_NAMESPACE_STREAM_UTILS namespace daq { namespace native_streaming { 
#define END_NAMESPACE_STREAM_UTILS }}

BEGIN_NAMESPACE_STREAM_UTILS

namespace boost_compatibility_utils
{
    using BoostHandler = std::function<void(const boost::system::error_code&)>;
    using WebsocketStream = boost::beast::websocket::stream<boost::beast::tcp_stream>;
    using WriteCallback = std::function<void(boost::beast::error_code ec, std::size_t written)>;

    void async_handshake(WebsocketStream& stream,
        const std::string& host,
        const std::string& target,
        const BoostHandler& handler);

    void async_accept(WebsocketStream& websocket, const BoostHandler& handler);

    void async_write(boost::beast::tcp_stream& stream,
                     boost::beast::http::request<boost::beast::http::string_body>& request,
					 WriteCallback callback);
}

END_NAMESPACE_STREAM_UTILS
