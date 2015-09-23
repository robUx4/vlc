# microdns

MICRODNS_VERSION := git
MICRODNS_HASH := HEAD
MICRODNS_GITURL := https://github.com/videolabs/libmicrodns.git

PKGS += microdns
ifeq ($(call need_pkg,"microdns"),)
PKGS_FOUND += microdns
endif

$(TARBALLS)/microdns-$(MICRODNS_HASH).tar.xz:
	$(call download_git,$(MICRODNS_GITURL),,$(MICRODNS_HASH))

.sum-microdns: $(TARBALLS)/microdns-$(HASH).tar.xz
	$(warning Not implemented.)
	touch $@

microdns: microdns-$(HASH).tar.xz .sum-microdns
	rm -Rf $@ $@-$(HASH)
	mkdir -p $@-$(HASH)
	$(XZCAT) "$<" | (cd $@-$(HASH) && tar xv --strip-components=1)
	$(MOVE)

.microdns: microdns
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
