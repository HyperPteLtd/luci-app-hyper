# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2022-2023 ImmortalWrt.org
# Copyright (C) 2026 Hyper Pte. Ltd.

include $(TOPDIR)/rules.mk

LUCI_TITLE:=Hyper for OpenWrt
LUCI_PKGARCH:=all
LUCI_DEPENDS:= \
	+sing-box \
	+firewall4 \
	+kmod-nft-tproxy \
	+ucode-mod-digest \
	+curl \
	+hyper-crypt

PKG_NAME:=luci-app-hyper
PKG_VERSION:=0.1.0
PKG_LICENSE:=GPL-2.0-only

# sing-box core: the official SagerNet OpenWrt package of exactly this version
# ships next to ours (scripts/fetch-sing-box.sh, hashes in scripts/sing-box.sha256).
# The generated config is tested against this version only; the older sing-box in
# the OpenWrt feeds (24.10: 1.12, 25.12: 1.13) does not satisfy the dependency.
HYPER_SINGBOX_VERSION:=1.14.4
EXTRA_DEPENDS:=sing-box (>=$(HYPER_SINGBOX_VERSION))

# htdocs/luci-static/hyper/ is a Vite build (ES modules, already minified).
# LuCI's jsmin / csstidy predate template literals and modern CSS and would
# corrupt it, so leave every .js / .css in this package as it is.
LUCI_MINIFY_JS:=0
LUCI_MINIFY_CSS:=0

define Package/luci-app-hyper/conffiles
/etc/config/hyper
/etc/hyper/session
/etc/hyper/checkin.json
/etc/hyper/ruleset/
/etc/hyper/resources/direct_list.txt
/etc/hyper/resources/proxy_list.txt
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
