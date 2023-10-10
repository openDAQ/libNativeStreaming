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

#include <spdlog/spdlog.h>
#include <functional>
#include <fmt/format.h>

#include <native_streaming/common.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

#define LOG(logCallback, logLevel, message, ...) \
    logCallback(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, logLevel, format(FMT_STRING(message), ##__VA_ARGS__).data());

#define NS_LOG_E(message, ...) LOG(logCallback, spdlog::level::err, message, ##__VA_ARGS__)

#define NS_LOG_I(message, ...) LOG(logCallback, spdlog::level::info, message, ##__VA_ARGS__)

#define NS_LOG_W(message, ...) LOG(logCallback, spdlog::level::warn, message, ##__VA_ARGS__)

#define NS_LOG_T(message, ...) LOG(logCallback, spdlog::level::trace, message, ##__VA_ARGS__)

#define NS_LOG_C(message, ...) LOG(logCallback, spdlog::level::critical, message, ##__VA_ARGS__)

#define NS_LOG_D(message, ...) LOG(logCallback, spdlog::level::debug, message, ##__VA_ARGS__)

using LogCallback = std::function<void(spdlog::source_loc location, spdlog::level::level_enum level, const char* msg)>;

class Logging
{
public:
    /// @brief provides callback function object used to log text messages indicated significant events
    /// @return log callback function object
    static LogCallback logCallback();
};

END_NAMESPACE_NATIVE_STREAMING
