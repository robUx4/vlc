# ZLIB
ZLIB_VERSION := 1.2.8
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

ifdef HAVE_SOLARIS
ZLIB_ECFLAGS = -fPIC -DPIC
endif

ifdef HAVE_VISUALSTUDIO
ifeq ($(VLC_CONFIGURATION),Debug)
STATIC_LIB=zlibstaticd
else
STATIC_LIB=zlibstatic
endif
endif

$(TARBALLS)/zlib-$(ZLIB_VERSION).tar.gz:
	$(call download,$(ZLIB_URL))

.sum-zlib: zlib-$(ZLIB_VERSION).tar.gz

zlib: zlib-$(ZLIB_VERSION).tar.gz .sum-zlib
	$(UNPACK)
	$(MOVE)

.zlib: zlib toolchain.cmake
ifdef HAVE_VISUALSTUDIO
	cd $< && $(HOSTVARS_CMAKE) $(CMAKE) -DSHARED=OFF .
	cd $< && msbuild.exe -p:Configuration=$(VLC_CONFIGURATION) -m -nologo INSTALL.vcxproj
	cd $< && cp "$(PREFIX)/lib/$(STATIC_LIB).lib" "$(PREFIX)/lib/z.lib"
else
	cd $< && $(HOSTVARS) $(ZLIB_CONFIG_VARS) CFLAGS="$(CFLAGS) $(ZLIB_ECFLAGS)" ./configure --prefix=$(PREFIX) --static
	cd $< && $(MAKE) install
endif
	touch $@
