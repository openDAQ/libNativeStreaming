# libNativeStreaming

The native streaming sends via the wire the same data packages as they are send internally in the SDK.

## Build commands:

```
cmake --preset gcc/debug
cmake --build build/gcc/debug
```

## TLS

Connections can be encrypted with TLS 1.2 or later. The TLS channel is optional: it is the only part of the
library that needs OpenSSL (1.1.1 or later), and it is compiled in only with the CMake option
`NATIVE_STREAMING_ENABLE_TLS`, which is `OFF` by default:

```
cmake --preset gcc/debug -DNATIVE_STREAMING_ENABLE_TLS=ON
cmake --build build/gcc/debug
```

With the option on, OpenSSL is linked publicly and the installed package config finds it for consumers. The
compile definition `NATIVE_STREAMING_ENABLE_TLS` is public too, so code using the TLS API guards it with
`#if NATIVE_STREAMING_ENABLE_TLS`. With the option off, the TLS API is compiled out and `tls.hpp` is not
installed.

Certificates and keys are passed as paths to PEM files, and the library builds the SSL contexts from them.

### Server

`Server::startTls()` listens for TLS connections, alongside `start()` on another port or instead of it:

```cpp
server->start(plainPort);
server->startTls(tlsPort, "server.crt", "server.key", "client-ca.crt");
```

The CA file is optional. If it is given, mutual TLS is enabled: clients must present a certificate signed by
that authority. A missing or malformed file is reported through the returned error code rather than thrown,
and so is a port the server already listens on (`address_in_use`).

### Client

A client connects over TLS when it is constructed with a `ClientTlsConfig`:

```cpp
ClientTlsConfig tlsConfig;
tlsConfig.caFile = "ca.crt";
tlsConfig.certFile = "client.crt"; // mutual TLS only
tlsConfig.keyFile = "client.key";  // mutual TLS only

auto client = std::make_shared<Client>(host, port, path, authentication,
                                       onNewSession, onResolveFail, onConnectFail, onHandshakeFail,
                                       ioContext, logCallback,
                                       tlsConfig, onTlsHandshakeFail);
```

The constructor reads the files. It throws `std::invalid_argument` if the configuration is inconsistent, for
example a certificate without a key, and `boost::system::system_error` if a file is missing or malformed.

`onTlsHandshakeFail` is optional. It is called when the TLS layer rejects the connection: the server was
reached, but the two sides do not trust each other, so retrying with the same files is pointless. Without it,
such failures are reported through `onHandshakeFail`.

### Modes

| Mode | Server | Client |
|---|---|---|
| Server authentication | `startTls(port, cert, key)` | `caFile` |
| Mutual TLS | `startTls(port, cert, key, clientCa)` | `caFile`, `certFile`, `keyFile` |
| Encryption without server authentication | `startTls(port, cert, key)` | `verifyServer = false` |

The server certificate is verified against the CA only; its host name is not checked. Without server
authentication any certificate the server presents is accepted, which leaves no protection against an active
man-in-the-middle: use it only where the server is trusted by other means.

The certificates under `test/secrets` are for the tests only.
