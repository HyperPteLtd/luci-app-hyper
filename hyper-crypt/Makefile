# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2026 Hyper Pte. Ltd.

include $(TOPDIR)/rules.mk

PKG_NAME:=hyper-crypt
PKG_VERSION:=0.1.0
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0-only
PKG_MAINTAINER:=Hyper Pte. Ltd.

include $(INCLUDE_DIR)/package.mk

define Package/hyper-crypt
  SECTION:=net
  CATEGORY:=Network
  TITLE:=X25519 / HKDF / AES-GCM helper for luci-app-hyper
  DEPENDS:=+libmbedtls
endef

define Package/hyper-crypt/description
  Generates ephemeral X25519 keys and opens the encrypted PPanel
  subscription envelope (X25519 + HKDF-SHA256 + AES-256-GCM).
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) -Wall -Wextra \
		-o $(PKG_BUILD_DIR)/hyper-crypt $(PKG_BUILD_DIR)/hyper-crypt.c \
		$(TARGET_LDFLAGS) -lmbedcrypto
endef

define Package/hyper-crypt/install
	$(INSTALL_DIR) $(1)/usr/libexec/hyper
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hyper-crypt $(1)/usr/libexec/hyper/hyper-crypt
endef

$(eval $(call BuildPackage,hyper-crypt))
