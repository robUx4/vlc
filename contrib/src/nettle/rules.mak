# Nettle

NETTLE_VERSION := 3.3
NETTLE_URL := ftp://ftp.gnu.org/gnu/nettle/nettle-$(NETTLE_VERSION).tar.gz

ifeq ($(call need_pkg,"nettle >= 2.7"),)
PKGS_FOUND += nettle
endif

$(TARBALLS)/nettle-$(NETTLE_VERSION).tar.gz:
	$(call download_pkg,$(NETTLE_URL),nettle)

.sum-nettle: nettle-$(NETTLE_VERSION).tar.gz

nettle: nettle-$(NETTLE_VERSION).tar.gz .sum-nettle
	$(UNPACK)
	cd $(UNPACK_DIR) && sed -i.orig -e 's/libnettle.a/nettle.lib/' Makefile.in
	cd $(UNPACK_DIR) && sed -i.orig -e 's/libhogweed.a/hogweed.lib/' Makefile.in
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

DEPS_nettle = gmp $(DEPS_gmp)

.nettle: nettle
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
