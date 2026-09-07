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

#include "ws_stream_impl.hpp"

BEGIN_NAMESPACE_NATIVE_STREAMING

std::shared_ptr<IWsStream> makePlainWsStream(std::shared_ptr<WebsocketStream> wsStream)
{
    return std::make_shared<WsStreamImpl<WebsocketStream>>(std::move(wsStream));
}

END_NAMESPACE_NATIVE_STREAMING
