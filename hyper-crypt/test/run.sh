#!/bin/sh
# Build hyper-crypt + the test-only seal tool against an mbedtls 3.6 tree and
# round-trip an envelope.
#   MBEDTLS=/path/to/mbedtls-3.6.x (with build/library/libmbedcrypto.a) hyper-crypt/test/run.sh

set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../src"
: "${MBEDTLS:?set MBEDTLS to an mbedtls 3.6 source tree}"
OUT="${OUT:-$(mktemp -d)}"
CFLAGS="-std=c99 -O2 -Wall -Wextra -Werror -I$MBEDTLS/include"
LIBS="$MBEDTLS/build/library/libmbedcrypto.a"

cc $CFLAGS -o "$OUT/hyper-crypt" "$SRC/hyper-crypt.c" $LIBS
cc $CFLAGS -o "$OUT/seal" "$HERE/seal.c" $LIBS

fail=0
check() { if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: got [$2] want [$3]"; fail=1; fi; }
field() { python3 -c "import json,sys; print(json.loads(sys.argv[1])[sys.argv[2]])" "$1" "$2"; }

kp="$("$OUT/hyper-crypt" keygen)"
priv="$(field "$kp" private_key)"
pub="$(field "$kp" public_key)"
check "keygen key length" "${#pub}" "43"

plain='{"nodes":[{"name":"日本 · 东京 01","protocol":"vless","port":443}]}'
# keygen prints url-safe base64 without padding; seal takes standard base64
pub_std="$(printf %s "$pub" | tr '_-' '/+')="
env="$(printf %s "$plain" | "$OUT/seal" "$pub_std" subscribe-v1)"
spk="$(field "$env" spk)"; salt="$(field "$env" salt)"; nonce="$(field "$env" nonce)"; data="$(field "$env" data)"

got="$(printf %s "$data" | HYPER_CRYPT_PRIVATE_KEY="$priv" "$OUT/hyper-crypt" open "$spk" "$salt" "$nonce" subscribe-v1)"
check "round trip" "$got" "$plain"

# url-safe base64 without padding is accepted too
data_url="$(printf %s "$data" | tr '+/' '-_' | tr -d '=')"
got="$(printf %s "$data_url" | HYPER_CRYPT_PRIVATE_KEY="$priv" "$OUT/hyper-crypt" open "$spk" "$salt" "$nonce" subscribe-v1)"
check "url-safe base64" "$got" "$plain"

# wrong info string must fail authentication
if printf %s "$data" | HYPER_CRYPT_PRIVATE_KEY="$priv" "$OUT/hyper-crypt" open "$spk" "$salt" "$nonce" other-info >/dev/null 2>&1; then
	check "wrong info rejected" "accepted" "rejected"; else check "wrong info rejected" "rejected" "rejected"; fi

# tampered ciphertext must fail
tampered="$(python3 -c "import base64,sys; b=bytearray(base64.b64decode(sys.argv[1])); b[0]^=1; print(base64.b64encode(bytes(b)).decode())" "$data")"
if printf %s "$tampered" | HYPER_CRYPT_PRIVATE_KEY="$priv" "$OUT/hyper-crypt" open "$spk" "$salt" "$nonce" subscribe-v1 >/dev/null 2>&1; then
	check "tamper rejected" "accepted" "rejected"; else check "tamper rejected" "rejected" "rejected"; fi

# another client's key must fail
other="$(field "$("$OUT/hyper-crypt" keygen)" private_key)"
if printf %s "$data" | HYPER_CRYPT_PRIVATE_KEY="$other" "$OUT/hyper-crypt" open "$spk" "$salt" "$nonce" subscribe-v1 >/dev/null 2>&1; then
	check "wrong key rejected" "accepted" "rejected"; else check "wrong key rejected" "rejected" "rejected"; fi

# usage errors
HYPER_CRYPT_PRIVATE_KEY= "$OUT/hyper-crypt" open a b c d </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
check "bad input exit code" "$rc" "3"
"$OUT/hyper-crypt" >/dev/null 2>&1 && rc=0 || rc=$?
check "usage exit code" "$rc" "2"

exit $fail
