# TagLib

TAGLIB_VERSION := 1.9.1
TAGLIB_URL := http://taglib.github.io/releases/taglib-$(TAGLIB_VERSION).tar.gz

PKGS += taglib
ifeq ($(call need_pkg,"taglib >= 1.9"),)
PKGS_FOUND += taglib
endif

ifdef HAVE_VISUALSTUDIO
BUILD_TARGET=-G "Visual Studio 12 2013 ARM"
endif

$(TARBALLS)/taglib-$(TAGLIB_VERSION).tar.gz:
	$(call download,$(TAGLIB_URL))

.sum-taglib: taglib-$(TAGLIB_VERSION).tar.gz

taglib: taglib-$(TAGLIB_VERSION).tar.gz .sum-taglib
	$(UNPACK)
	$(APPLY) $(SRC)/taglib/taglib-pc.patch
	$(APPLY) $(SRC)/taglib/0002-Rewrote-ByteVector-replace-simpler.patch
	$(MOVE)

.taglib: taglib toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) \
		-DENABLE_STATIC:BOOL=ON \
		-DWITH_ASF:BOOL=ON \
		-DWITH_MP4:BOOL=ON \
		$(BUILD_TARGET) .
ifdef HAVE_VISUALSTUDIO
	cd $< && msbuild.exe -p:Configuration=Release -p:Platform=ARM INSTALL.vcxproj
	cd $< && cp taglib/Release/tag.lib "$(PREFIX)/lib/libtag.a"
else
	cd $< && $(MAKE) install
endif
	touch $@
