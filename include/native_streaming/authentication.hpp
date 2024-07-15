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
enum class AuthenticationType
{
    Anonymous,
    Basic
};


/// @brief A structure for holding authentication information
class Authentication
{
public:
    Authentication();
    Authentication(const std::string& username, const std::string& password);

    /// @breif Parses a header string into authentication information structure. For example
    /// basic Authorization header is usually encoded as "Basic base64(username:password)"
    static Authentication fromHeader(const std::string& header);

    /// @brief Authentication type (anonymous, username / password, ...)
    AuthenticationType getType() const;

    /// @brief Username as a string
    std::string getUsername() const;

    /// @brief Plain text password as a string
    std::string getPassword() const;

    /// @brief Returns encoded (Authorization) header as a string. For example basic Authorization
    /// header is usually encoded as "Basic base64(username:password)"
    std::string getEncodedHeader() const;

private:
    AuthenticationType type = AuthenticationType::Anonymous;
    std::string username;
    std::string password;
};

END_NAMESPACE_NATIVE_STREAMING
