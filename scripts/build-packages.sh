#!/bin/sh
# Build test packages with the official OpenWrt SDKs inside Docker:
#   24.10 -> .ipk (opkg), 25.12 -> .apk (apk-tools 3).
#
#   scripts/build-packages.sh [target ...]          default target: x86/64
#   e.g. scripts/build-packages.sh x86/64 mediatek/filogic rockchip/armv8
#
#   RELEASES="24.10.8 25.12.6"   OpenWrt releases to build for
#
# Output: dist/openwrt-<release>/<files> plus sha256sums. luci-app-hyper is
# architecture independent; hyper-crypt is built for each target's CPU; sing-box
# is the official SagerNet OpenWrt package (scripts/fetch-sing-box.sh), not built.
#
# The SDK needs a case-sensitive file system, so it lives on the Docker host
# (/var/tmp/hyper-sdk, inside the colima / Docker Desktop VM on macOS), not on
# the macOS disk. Sources go in and packages come out as tar streams, so no
# folder sharing is needed (colima shares only the home folder by default).
# Runs on Apple Silicon through amd64 emulation (slow, but the packages are
# small). Remove everything with:
#   docker rm -f hyper-sdk-builder && docker run --rm -v /var/tmp:/t debian:12-slim rm -rf /t/hyper-sdk

set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RELEASES="${RELEASES:-24.10.8 25.12.6}"
TARGETS="${*:-x86/64}"
NAME=hyper-sdk-builder
IMAGE=debian:12-slim
WORK=/var/tmp/hyper-sdk

echo "== web UI"
npm --prefix "$ROOT/web" run build >/dev/null
test -f "$ROOT/luci-app-hyper/htdocs/luci-static/hyper/index.html"

mkdir -p "$ROOT/dist"

if ! docker container inspect "$NAME" >/dev/null 2>&1; then
	echo "== builder container ($IMAGE, linux/amd64)"
	docker run -d --name "$NAME" --platform linux/amd64 \
		-v "$WORK:/work" "$IMAGE" sleep infinity >/dev/null
fi
docker start "$NAME" >/dev/null

docker exec "$NAME" sh -c '
	set -e
	if ! id build >/dev/null 2>&1; then
		export DEBIAN_FRONTEND=noninteractive
		apt-get update -qq
		apt-get install -y -qq --no-install-recommends \
			build-essential ca-certificates file gawk gettext git libncurses-dev libssl-dev \
			python3 python3-setuptools rsync unzip wget xz-utils zstd bzip2 zlib1g-dev >/dev/null
		useradd -m build
	fi
	mkdir -p /work && chown build: /work
'

echo "== copy sources into the container"
# macOS (bsdtar) would add Apple extended attributes that GNU tar warns about
case "$(tar --version 2>/dev/null)" in *bsdtar*) TAR_NOMETA="--no-xattrs --no-mac-metadata" ;; *) TAR_NOMETA="" ;; esac
# shellcheck disable=SC2086
COPYFILE_DISABLE=1 tar $TAR_NOMETA -C "$ROOT" --exclude='._*' --exclude='.DS_Store' --exclude='*.o' \
	--exclude='hyper-crypt/src/hyper-crypt' --exclude='hyper-crypt/test/out' \
	-cf - luci-app-hyper hyper-crypt scripts/sdk-build.sh |
	docker exec -i "$NAME" sh -c 'rm -rf /work/src && mkdir -p /work/src && tar -C /work/src -xf - && chown -R build: /work/src'

for release in $RELEASES; do
	for target in $TARGETS; do
		echo "== OpenWrt $release $target"
		if ! docker exec -u build "$NAME" sh /work/src/scripts/sdk-build.sh "$release" "$target"; then
			echo "build failed; log: docker exec $NAME tail -n 80 /work/$release-$(echo "$target" | tr / -)/build.log" >&2
			exit 1
		fi
	done
	docker exec "$NAME" sh -c "
		set -e
		out=/work/out/openwrt-$release
		rm -rf \$out && mkdir -p \$out
		for d in /work/$release-*/sdk/bin/packages/*/hyper; do cp \$d/*.ipk \$d/*.apk \$out/ 2>/dev/null || true; done
		cd \$out && sha256sum \$(ls *.ipk *.apk 2>/dev/null) > sha256sums
	"
	rm -rf "$ROOT/dist/openwrt-$release"
	docker exec "$NAME" tar -C /work/out -cf - "openwrt-$release" | tar -C "$ROOT/dist" -xf -

	# sing-box core: the official SagerNet package for each target's CPU, downloaded as is
	case "$release" in 2[0-4].*) ext=ipk ;; *) ext=apk ;; esac
	for target in $TARGETS; do
		arch="$(curl -fsSL "https://downloads.openwrt.org/releases/$release/targets/$target/profiles.json" |
			python3 -I -c 'import json, sys; print(json.load(sys.stdin)["arch_packages"])')"
		sh "$ROOT/scripts/fetch-sing-box.sh" "$ext" "$ROOT/dist/.sing-box" "$arch"
		cp "$ROOT"/dist/.sing-box/sing-box_*_openwrt_"$arch.$ext" "$ROOT/dist/openwrt-$release/"
	done
	(
		cd "$ROOT/dist/openwrt-$release"
		if command -v sha256sum >/dev/null 2>&1; then sha256sum ./*."$ext"; else shasum -a 256 ./*."$ext"; fi |
			sed 's#  \./#  #' > sha256sums
	)
done

echo "== done"
ls -l "$ROOT"/dist/openwrt-*/
