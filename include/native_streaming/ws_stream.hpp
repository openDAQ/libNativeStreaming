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
#include <boost/asio/streambuf.hpp>
#include <boost/container/small_vector.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

#if !defined(NDEBUG) && defined(_MSC_VER) && UINTPTR_MAX == 0xFFFFFFFF

struct __declspec(align(16)) const_buffer_small_vector_type : boost::container::small_vector<boost::asio::const_buffer, 16>
{
};

using const_buffer_small_vector = const_buffer_small_vector_type;

#else

using const_buffer_small_vector = boost::container::small_vector<boost::asio::const_buffer, 16>;

#endif

/// @brief transport-neutral view of a web-socket stream.
/// Session, AsyncReader and AsyncWriter operate on connections through this interface only.
/// An implementation is created by Client or Server once the connection is established, and wraps a concrete
/// boost::beast::websocket::stream over either a plaintext or an encrypted transport.
/// All member functions must be invoked from the execution context of the underlying stream.
class IWsStream
{
public:
    virtual ~IWsStream() = default;

    /// @brief applies the connection settings the library relies on: timeouts are handled by the
    /// web-socket stream rather than by the transport below it, the binary message format is used,
    /// and the role-specific timeout policy is applied
    /// @param role determines whether the stream belongs to a server or to a client
    virtual void configure(boost::beast::role_type role) = 0;

    /// @brief asynchronously reads into the buffer until it holds at least the requested amount of bytes
    /// @param buffer buffer to read into
    /// @param bytesToRead least amount of bytes to read
    /// @param onReadCallback callback to be called on read completion
    virtual void asyncReadAtLeast(boost::asio::streambuf& buffer,
                                  std::size_t bytesToRead,
                                  OnRWCallback onReadCallback) = 0;

    /// @brief asynchronously writes a sequence of buffers as a single web-socket message.
    /// The sequence itself is copied, so the caller is free to discard it once the call returns; the
    /// memory the buffers refer to must stay valid until the callback is called.
    /// @param buffers sequence of buffers making up the message
    /// @param onWriteCallback callback to be called on write completion
    virtual void asyncWrite(const const_buffer_small_vector& buffers,
                            OnRWCallback onWriteCallback) = 0;

    /// @brief asynchronously performs the web-socket closing handshake
    /// @param code web-socket close reason code
    /// @param onClosedCallback callback to be called on close completion
    virtual void asyncClose(boost::beast::websocket::close_code code,
                            OnCompleteCallback onClosedCallback) = 0;

    /// @brief asynchronously sends an unsolicited web-socket pong frame
    /// @param payload pong payload
    /// @param onSentCallback callback to be called on send completion
    virtual void asyncPong(const std::string& payload, OnCompleteCallback onSentCallback) = 0;

    /// @brief sets a callback to be called when a web-socket control frame is received
    /// @param controlCallback callback
    virtual void setControlCallback(OnControlCallback controlCallback) = 0;

    /// @brief returns true if the web-socket stream is open, false otherwise
    virtual bool isOpen() const = 0;

    /// @brief cancels all outstanding asynchronous operations on the underlying socket
    virtual void cancelLowestLayer() = 0;
};

/// @brief wraps a plaintext web-socket stream into the transport-neutral interface
/// @param wsStream connected web-socket stream with a completed handshake
/// @return pointer to the created wrapper
std::shared_ptr<IWsStream> makePlainWsStream(std::shared_ptr<WebsocketStream> wsStream);

END_NAMESPACE_NATIVE_STREAMING
