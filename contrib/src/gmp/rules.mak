# GNU Multiple Precision Arithmetic

GMP_VERSION := 6.0.0
GMP_URL := https://gmplib.org/download/gmp-$(GMP_VERSION)/gmp-$(GMP_VERSION).tar.bz2

$(TARBALLS)/gmp-$(GMP_VERSION).tar.bz2:
	$(call download,$(GMP_URL))

.sum-gmp: gmp-$(GMP_VERSION).tar.bz2

gmp: gmp-$(GMP_VERSION).tar.bz2 .sum-gmp
	$(UNPACK)
	$(APPLY) $(SRC)/gmp/thumb.patch
	$(APPLY) $(SRC)/gmp/clang.patch
	$(MOVE)

ifdef HAVE_VISUALSTUDIO
ifeq ($(ARCH),arm)
GMPCONF += --disable-assembly
endif
endif

.gmp: gmp
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(GMPCONF)
	cd $< && $(MAKE) install
	touch $@
