# PNG
PNG_VERSION := 1.6.29
PNG_URL := $(SF)/libpng/libpng16/$(PNG_VERSION)/libpng-$(PNG_VERSION).tar.xz

PKGS += png
ifeq ($(call need_pkg,"libpng >= 1.5.4"),)
PKGS_FOUND += png
endif

$(TARBALLS)/libpng-$(PNG_VERSION).tar.xz:
	$(call download_pkg,$(PNG_URL),png)

.sum-png: libpng-$(PNG_VERSION).tar.xz

png: libpng-$(PNG_VERSION).tar.xz .sum-png
	$(UNPACK)
	$(APPLY) $(SRC)/png/winrt.patch
	$(APPLY) $(SRC)/png/bins.patch
	$(APPLY) $(SRC)/png/automake.patch
ifdef HAVE_VISUALSTUDIO
	$(APPLY) $(SRC)/png/msvc.patch
endif
	$(call pkg_static,"libpng.pc.in")
	$(MOVE)

DEPS_png = zlib $(DEPS_zlib)

ifdef HAVE_VISUALSTUDIO
CONF = --disable-arm-neon
endif

.png: png
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(CONF)
	cd $< && $(MAKE) install
	touch $@
