# LIBARCHIVE
LIBARCHIVE_VERSION := 3.1.2
LIBARCHIVE_URL := http://www.libarchive.org/downloads/libarchive-$(LIBARCHIVE_VERSION).tar.gz

PKGS += libarchive
ifeq ($(call need_pkg,"libarchive >= 3.1.0"),)
PKGS_FOUND += libarchive
endif

$(TARBALLS)/libarchive-$(LIBARCHIVE_VERSION).tar.gz:
	$(call download,$(LIBARCHIVE_URL))

.sum-libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.gz

libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.gz .sum-libarchive
	$(UNPACK)
	$(APPLY) $(SRC)/libarchive/0001-Fix-build-failure-without-STATVFS.patch
	$(APPLY) $(SRC)/libarchive/0001-Use-the-same-__LA_MODE_T-as-in-the-header.patch
ifdef HAVE_VISUALSTUDIO
	$(APPLY) $(SRC)/libarchive/0002-fix-compilation-with-MSVC.patch
	$(APPLY) $(SRC)/libarchive/0003-do-not-compile-code-that-s-not-available-in-the-Wind.patch
endif
	$(MOVE)

.libarchive: libarchive
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) \
		--disable-bsdcpio --disable-bsdtar --without-nettle --without-bz2lib \
		--without-xml2 --without-lzmadec --without-iconv --without-expat
	cd $< && $(MAKE) install
	touch $@
