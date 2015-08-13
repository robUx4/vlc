# mpg123
MPG123_VERSION := 1.21.0
MPG123_URL := $(SF)/mpg123/$(MPG123_VERSION)/mpg123-$(MPG123_VERSION).tar.bz2

PKGS += mpg123
ifeq ($(call need_pkg,"mpg123"),)
PKGS_FOUND += mpg123
endif

$(TARBALLS)/mpg123-$(MPG123_VERSION).tar.bz2:
	$(call download,$(MPG123_URL))

.sum-mpg123: mpg123-$(MPG123_VERSION).tar.bz2

mpg123: mpg123-$(MPG123_VERSION).tar.bz2 .sum-mpg123
	$(UNPACK)
	$(APPLY) $(SRC)/mpg123/no-programs.patch
ifdef HAVE_WINRT
	$(APPLY) $(SRC)/mpg123/winstore.patch
endif
ifdef HAVE_VISUALSTUDIO
	$(APPLY) $(SRC)/mpg123/msvc.patch
endif
	$(MOVE)

.mpg123: mpg123
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
