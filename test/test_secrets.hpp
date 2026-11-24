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

#include <native_streaming/common.hpp>

#if NATIVE_STREAMING_ENABLE_TLS

BEGIN_NAMESPACE_NATIVE_STREAMING

/// @brief the TLS fixtures, copied next to the test executable at build time. Regenerate them with
/// test/secrets/gen_certs.sh
namespace test_secrets
{
    /// the authority the tests trust
    inline constexpr const char* CaCert = "secrets/ca.crt";

    /// server certificate and key, issued by CaCert
    inline constexpr const char* ServerCert = "secrets/server.crt";
    inline constexpr const char* ServerKey = "secrets/server.key";

    /// client certificate and key, issued by CaCert, for mutual TLS
    inline constexpr const char* ClientCert = "secrets/client.crt";
    inline constexpr const char* ClientKey = "secrets/client.key";

    /// an unrelated authority, which trusts neither of the certificates above
    inline constexpr const char* OtherCaCert = "secrets/other-ca.crt";

    /// a path no fixture is at, for the cases where a secret cannot be loaded
    inline constexpr const char* MissingCert = "secrets/there-is-no-such-file.crt";
}

END_NAMESPACE_NATIVE_STREAMING

#endif
