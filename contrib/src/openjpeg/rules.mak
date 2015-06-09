# jpeg

OPENJPEG_VERSION := 1.5.0
OPENJPEG_URL := http://sourceforge.net/projects/openjpeg.mirror/files/$(OPENJPEG_VERSION)/openjpeg-$(OPENJPEG_VERSION).tar.gz/download

$(TARBALLS)/openjpeg-$(OPENJPEG_VERSION).tar.gz:
	$(call download,$(OPENJPEG_URL))

.sum-openjpeg: openjpeg-$(OPENJPEG_VERSION).tar.gz

openjpeg: openjpeg-$(OPENJPEG_VERSION).tar.gz .sum-openjpeg
	$(UNPACK)
ifdef HAVE_VISUALSTUDIO
	dos2unix $(SRC)/openjpeg/*.patch
	$(APPLY) $(SRC)/openjpeg/msvc.patch
endif
	$(APPLY) $(SRC)/openjpeg/freebsd.patch
	$(APPLY) $(SRC)/openjpeg/restrict.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.openjpeg: openjpeg
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -DOPJ_STATIC" ./configure --enable-png=no --enable-tiff=no $(HOSTCONF)
	cd $< && $(MAKE) -C libopenjpeg -j1 install
	touch $@
