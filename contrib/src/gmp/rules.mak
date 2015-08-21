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
	$(APPLY) $(SRC)/gmp/ppc64.patch
	$(MOVE)

GMP_ENV := $(HOSTVARS)

ifdef HAVE_VISUALSTUDIO
ifeq ($(ARCH),arm)
GMPCONF += --disable-assembly
endif
ifeq ($(ARCH),x86_64)
GMPCONF += --disable-assembly
GMP_ENV += gmp_asm_syntax_testing=no
endif
endif

.gmp: gmp
	cd $< && $(GMP_ENV) ./configure $(HOSTCONF) $(GMPCONF)
	cd $< && $(MAKE) install
	touch $@
