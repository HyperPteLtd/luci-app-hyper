#!/bin/sh
# Download the official SagerNet sing-box OpenWrt package (no compiling) for
# each package arch, checked against scripts/sing-box.sha256.
#
#   scripts/fetch-sing-box.sh <ipk|apk> <out dir> <arch> [arch ...]
#   e.g. scripts/fetch-sing-box.sh ipk dist/openwrt-24.10.8 aarch64_cortex-a53
#
# The version comes from HYPER_SINGBOX_VERSION in luci-app-hyper/Makefile.

set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ext="$1"
out="$2"
shift 2
[ "$ext" = ipk ] || [ "$ext" = apk ] || { echo "first argument must be ipk or apk" >&2; exit 2; }
[ $# -gt 0 ] || { echo "no arch given" >&2; exit 2; }

version="$(sed -n 's/^HYPER_SINGBOX_VERSION:=//p' "$ROOT/luci-app-hyper/Makefile")"
[ -n "$version" ] || { echo "HYPER_SINGBOX_VERSION not found in luci-app-hyper/Makefile" >&2; exit 1; }

sha256() {
	if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

mkdir -p "$out"
for arch in "$@"; do
	name="sing-box_${version}_openwrt_${arch}.${ext}"
	want="$(awk -v n="$name" '$2 == n { print $1 }' "$ROOT/scripts/sing-box.sha256")"
	[ -n "$want" ] || { echo "$name is not listed in scripts/sing-box.sha256" >&2; exit 1; }

	if [ ! -f "$out/$name" ] || [ "$(sha256 "$out/$name")" != "$want" ]; then
		echo "   download $name"
		curl -fsSL --retry 3 -o "$out/$name.part" "https://github.com/SagerNet/sing-box/releases/download/v${version}/${name}"
		mv "$out/$name.part" "$out/$name"
	fi
	got="$(sha256 "$out/$name")"
	if [ "$got" != "$want" ]; then
		rm -f "$out/$name"
		echo "checksum mismatch for $name: got $got, want $want" >&2
		exit 1
	fi
done
