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

BEGIN_NAMESPACE_NATIVE_STREAMING

std::shared_ptr<IWsStream> makeTlsWsStream(std::shared_ptr<TlsWebsocketStream> wsStream)
{
    return std::make_shared<WsStreamImpl<TlsWebsocketStream>>(std::move(wsStream));
}

END_NAMESPACE_NATIVE_STREAMING

#endif // NATIVE_STREAMING_ENABLE_TLS
