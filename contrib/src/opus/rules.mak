# opus

OPUS_VERSION := 1.1.3

OPUS_URL := http://downloads.xiph.org/releases/opus/opus-$(OPUS_VERSION).tar.gz

PKGS += opus
ifeq ($(call need_pkg,"opus >= 0.9.14"),)
PKGS_FOUND += opus
endif

$(TARBALLS)/opus-$(OPUS_VERSION).tar.gz:
	$(call download_pkg,$(OPUS_URL),opus)

.sum-opus: opus-$(OPUS_VERSION).tar.gz

opus: opus-$(OPUS_VERSION).tar.gz .sum-opus
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

OPUS_CFLAGS=$(CFLAGS)
OPUS_CONF= --disable-extra-programs --disable-doc
ifndef HAVE_FPU
OPUS_CONF += --enable-fixed-point
endif
ifeq ($(ARCH),arm)
OPUS_CONF += --disable-asm
OPUS_CFLAGS+= -fno-fast-math
endif
ifdef HAVE_VISUALSTUDIO
OPUS_CONF += --disable-rtcd
endif

ifdef HAVE_VISUALSTUDIO
OPUS_CFLAGS+= -DUSE_ALLOCA
endif

.opus: opus
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(OPUS_CONF) CFLAGS="$(OPUS_CFLAGS)"
	cd $< && $(MAKE) install
	touch $@
