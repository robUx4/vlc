# ZLIB
ZLIB_VERSION := 1.2.11
ZLIB_URL := $(SF)/libpng/zlib-$(ZLIB_VERSION).tar.gz

PKGS += zlib
ifeq ($(call need_pkg,"zlib"),)
PKGS_FOUND += zlib
endif

ifeq ($(shell uname),Darwin) # zlib tries to use libtool on Darwin
ifdef HAVE_CROSS_COMPILE
ZLIB_CONFIG_VARS=CHOST=$(HOST)
endif
endif

$(TARBALLS)/zlib-$(ZLIB_VERSION).tar.gz:
	$(call download_pkg,$(ZLIB_URL),zlib)

.sum-zlib: zlib-$(ZLIB_VERSION).tar.gz

zlib: zlib-$(ZLIB_VERSION).tar.gz .sum-zlib
	$(UNPACK)
	$(APPLY) $(SRC)/zlib/no-shared.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/zlib/mingw.patch
endif
	$(MOVE)

.zlib: zlib
	cd $< && $(HOSTVARS_PIC) $(ZLIB_CONFIG_VARS) ./configure --prefix=$(PREFIX) --static
	cd $< && $(MAKE) install
ifdef HAVE_VISUALSTUDIO
	cd $< && cp "$(PREFIX)/lib/libz.a" "$(PREFIX)/lib/z.lib"
endif
	touch $@
