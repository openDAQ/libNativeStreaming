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

#include <native_streaming/ws_stream.hpp>

#include <stdexcept>
#include <string>

#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/beast/ssl.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

/// @brief web-socket stream running over a TLS-encrypted transport.
/// boost::beast::ssl_stream is used rather than boost::asio::ssl::stream because
/// boost::beast::websocket::stream requires a movable next layer: the server moves an accepted
/// socket into the stream, and asio's SSL stream is not movable.
using TlsWebsocketStream = boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>;

/// @brief wraps a TLS web-socket stream into the transport-neutral interface
/// @param wsStream connected web-socket stream with completed TLS and web-socket handshakes
/// @return pointer to the created wrapper
std::shared_ptr<IWsStream> makeTlsWsStream(std::shared_ptr<TlsWebsocketStream> wsStream);

/// @brief applies the TLS options common to both sides: the obsolete protocol versions are
/// disabled, leaving TLS 1.2 and later
/// @param context SSL context to apply the options to
inline void applyCommonTlsOptions(boost::asio::ssl::context& context)
{
    context.set_options(boost::asio::ssl::context::default_workarounds
                        | boost::asio::ssl::context::no_sslv2
                        | boost::asio::ssl::context::no_sslv3
                        | boost::asio::ssl::context::no_tlsv1
                        | boost::asio::ssl::context::no_tlsv1_1);
}

/// @brief builds a client-side TLS context which authenticates the server against the given
/// certificate authority.
/// @param caFile path to a PEM file of trusted CA certificates used to verify the server. Required:
/// if empty, std::invalid_argument is thrown
/// @param certFile path to the client certificate chain (PEM), for mutual TLS. Optional, but if set
/// keyFile must be set as well
/// @param keyFile path to the client private key (PEM), for mutual TLS. Optional, but if set
/// certFile must be set as well
/// @return the created SSL context
/// @throw std::invalid_argument caFile is empty, or exactly one of certFile and keyFile is set
/// @throw boost::system::system_error a certificate or key file is missing or malformed
inline boost::asio::ssl::context makeClientTlsContext(const std::string& caFile,
                                                      const std::string& certFile = {},
                                                      const std::string& keyFile = {})
{
    if (caFile.empty())
        throw std::invalid_argument("makeClientTlsContext: caFile is required");

    if (certFile.empty() != keyFile.empty())
        throw std::invalid_argument("makeClientTlsContext: certFile and keyFile must both be set "
                                    "(mutual TLS) or both be empty (server-only authentication)");

    boost::asio::ssl::context context(boost::asio::ssl::context::tls_client);
    applyCommonTlsOptions(context);

    context.load_verify_file(caFile);
    context.set_verify_mode(boost::asio::ssl::verify_peer);

    if (!certFile.empty())
    {
        context.use_certificate_chain_file(certFile);
        context.use_private_key_file(keyFile, boost::asio::ssl::context::pem);
    }

    return context;
}

/// @brief builds a client-side TLS context which does not authenticate the server.
/// The connection is encrypted, but whatever certificate the server presents is accepted: this
/// provides confidentiality without authentication, and no protection against an active
/// man-in-the-middle. Use it only where the server is trusted by other means.
/// Because the server is not authenticated, presenting a client certificate to it serves no purpose,
/// and none is configured.
/// @return the created SSL context
inline boost::asio::ssl::context makeClientTlsContextWithoutVerification()
{
    boost::asio::ssl::context context(boost::asio::ssl::context::tls_client);
    applyCommonTlsOptions(context);

    context.set_verify_mode(boost::asio::ssl::verify_none);

    return context;
}

/// @brief builds a server-side TLS context
/// @param certFile path to the server certificate chain (PEM). Required
/// @param keyFile path to the server private key (PEM). Required
/// @param caFile path to a PEM file of trusted CA certificates used to verify client certificates.
/// If non-empty, mutual TLS is enabled: clients must present a certificate signed by one of these
/// authorities. If empty, client certificates are not requested
/// @return the created SSL context
/// @throw std::invalid_argument certFile or keyFile is empty
/// @throw boost::system::system_error a certificate or key file is missing or malformed
inline boost::asio::ssl::context makeServerTlsContext(const std::string& certFile,
                                                      const std::string& keyFile,
                                                      const std::string& caFile = {})
{
    if (certFile.empty() || keyFile.empty())
        throw std::invalid_argument("makeServerTlsContext: certFile and keyFile are required");

    boost::asio::ssl::context context(boost::asio::ssl::context::tls_server);
    applyCommonTlsOptions(context);

    context.use_certificate_chain_file(certFile);
    context.use_private_key_file(keyFile, boost::asio::ssl::context::pem);

    if (!caFile.empty())
    {
        context.load_verify_file(caFile);
        context.set_verify_mode(boost::asio::ssl::verify_peer | boost::asio::ssl::verify_fail_if_no_peer_cert);
    }

    return context;
}

END_NAMESPACE_NATIVE_STREAMING

#endif // NATIVE_STREAMING_ENABLE_TLS
