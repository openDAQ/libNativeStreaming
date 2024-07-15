/*
 * Copyright 2022-2024 openDAQ d.o.o.
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

#include <native_streaming/common.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

/// @brief Authentication type enum describes how to encode authentication information
class Base64
{
public:

    /// @brief encode string in base64 format
    /// @param input represents a plain input string
    static std::string encode(const std::string& input);

    /// @brief decode string from base64 format
    /// @param input represents a base64 encoded string
    static std::string decode(const std::string& input);

private:
    Base64() = default;
};

END_NAMESPACE_NATIVE_STREAMING
