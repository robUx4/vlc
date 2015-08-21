# Nettle

NETTLE_VERSION := 2.7.1
NETTLE_URL := ftp://ftp.gnu.org/gnu/nettle/nettle-$(NETTLE_VERSION).tar.gz

ifeq ($(call need_pkg,"nettle >= 2.7"),)
PKGS_FOUND += nettle
endif

$(TARBALLS)/nettle-$(NETTLE_VERSION).tar.gz:
	$(call download,$(NETTLE_URL))

.sum-nettle: nettle-$(NETTLE_VERSION).tar.gz

nettle: nettle-$(NETTLE_VERSION).tar.gz .sum-nettle
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
ifdef HAVE_VISUALSTUDIO
	$(APPLY) $(SRC)/nettle/msvc.patch
endif
	$(MOVE)

DEPS_nettle = gmp $(DEPS_gmp)

ifdef HAVE_VISUALSTUDIO
ifeq ($(ARCH),arm)
NETTLECONF += --disable-assembler
endif
endif

.nettle: nettle
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(NETTLECONF)
	cd $< && $(MAKE) install
	touch $@
