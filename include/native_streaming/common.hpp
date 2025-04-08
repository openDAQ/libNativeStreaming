/*
 * Copyright 2022-2025 openDAQ d.o.o.
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

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <thread>
#include <spdlog/spdlog.h>
#include <fmt/format.h>

#define BEGIN_NAMESPACE_NATIVE_STREAMING \
    namespace daq::native_streaming       \
    {

#define END_NAMESPACE_NATIVE_STREAMING }

BEGIN_NAMESPACE_NATIVE_STREAMING

using WebsocketStream = boost::beast::websocket::stream<boost::beast::tcp_stream>;
using OnRWCallback = std::function<void(const boost::system::error_code&, std::size_t)>;
using OnCompleteCallback = std::function<void(const boost::system::error_code&)>;
using OnConnectionAliveCallback = std::function<void()>;
using OnWriteTaskTimedOutCallback = std::function<void()>;

END_NAMESPACE_NATIVE_STREAMING
