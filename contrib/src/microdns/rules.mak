# libmicrodns

LIBMICRODNS_VERSION := 0.0.3
LIBMICRODNS_URL := https://github.com/videolabs/libmicrodns/releases/download/$(LIBMICRODNS_VERSION)/microdns-$(LIBMICRODNS_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += microdns
endif
ifeq ($(call need_pkg,"microdns >= 0.0.1"),)
PKGS_FOUND += microdns
endif

$(TARBALLS)/microdns-$(LIBMICRODNS_VERSION).tar.gz:
	$(call download_pkg,$(LIBMICRODNS_URL),microdns)

.sum-microdns: $(TARBALLS)/microdns-$(LIBMICRODNS_VERSION).tar.gz

microdns: microdns-$(LIBMICRODNS_VERSION).tar.gz .sum-microdns
	$(UNPACK)
	$(APPLY) $(SRC)/microdns/vla.patch
	$(APPLY) $(SRC)/microdns/unused-header.patch
	$(APPLY) $(SRC)/microdns/0001-mdns_recv-do-not-return-freed-memory-in-case-of-erro.patch
	$(APPLY) $(SRC)/microdns/0002-mdns_recv-initialize-the-max-size-of-buffer-to-read-.patch
	$(MOVE)

.microdns: microdns
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
