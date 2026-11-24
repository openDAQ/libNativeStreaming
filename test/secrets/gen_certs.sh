#!/usr/bin/env bash
# Regenerates the TLS fixtures used by the tests. The generated files are committed, so this only
# has to be run when they expire or when the set of them changes.
#
# The keys are unencrypted: the library configures no passphrase callback.
# The server certificate carries IP:127.0.0.1 / DNS:localhost subject alternative names. The client
# does not verify the server's identity against the address it connected to - only that the chain
# leads to the configured authority - so the names are there for the sake of the fixture being
# well-formed rather than because a test depends on them.
set -euo pipefail
cd "$(dirname "$0")"
DAYS=3650

gen_ca () { # name CN
  openssl req -x509 -newkey rsa:2048 -nodes -keyout "$1.key" -out "$1.crt" \
    -days "$DAYS" -subj "/CN=$2"
}

gen_leaf () { # name CN ca extfile
  openssl req -newkey rsa:2048 -nodes -keyout "$1.key" -out "$1.csr" -subj "/CN=$2"
  openssl x509 -req -in "$1.csr" -CA "$3.crt" -CAkey "$3.key" -CAcreateserial \
    -out "$1.crt" -days "$DAYS" ${4:+-extfile "$4"}
  rm -f "$1.csr"
}

cat > server_ext.cnf <<'EXT'
subjectAltName = IP:127.0.0.1, DNS:localhost
extendedKeyUsage = serverAuth
EXT

cat > client_ext.cnf <<'EXT'
extendedKeyUsage = clientAuth
EXT

# the authority the tests trust, and the leaves issued by it
gen_ca   ca      "libNativeStreaming Test CA"
gen_leaf server  "native-streaming-test-server" ca server_ext.cnf
gen_leaf client  "native-streaming-test-client" ca client_ext.cnf

# an unrelated authority, used to prove that an untrusted peer is rejected
gen_ca   other-ca "libNativeStreaming Other CA"

rm -f server_ext.cnf client_ext.cnf ca.srl other-ca.srl
echo "Generated: $(ls *.crt *.key | tr '\n' ' ')"
