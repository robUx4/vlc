# libmicrodns

LIBMICRODNS_VERSION := 0.0.2
LIBMICRODNS_URL := https://github.com/videolabs/libmicrodns/releases/download/$(LIBMICRODNS_VERSION)/microdns-$(LIBMICRODNS_VERSION).tar.gz

ifeq ($(call need_pkg,"microdns >= 0.0.1"),)
PKGS_FOUND += libmicrodns
endif

$(TARBALLS)/microdns-$(LIBMICRODNS_VERSION).tar.gz:
	$(call download,$(LIBMICRODNS_URL))

.sum-microdns: $(TARBALLS)/microdns-$(LIBMICRODNS_VERSION).tar.gz

microdns: microdns-$(LIBMICRODNS_VERSION).tar.gz .sum-microdns
	$(UNPACK)
	mv microdns-$(LIBMICRODNS_VERSION) microdns

.microdns: microdns
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
