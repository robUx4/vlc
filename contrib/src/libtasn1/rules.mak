# libtasn1

LIBTASN1_VERSION := 4.8
LIBTASN1_URL := $(GNU)/libtasn1/libtasn1-$(LIBTASN1_VERSION).tar.gz

ifeq ($(call need_pkg,"libtasn1 >= 4.3"),)
PKGS_FOUND += libtasn1
endif

$(TARBALLS)/libtasn1-$(LIBTASN1_VERSION).tar.gz:
	$(call download_pkg,$(LIBTASN1_URL),libtasn1)

.sum-libtasn1: libtasn1-$(LIBTASN1_VERSION).tar.gz

libtasn1: libtasn1-$(LIBTASN1_VERSION).tar.gz .sum-libtasn1
	$(UNPACK)
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/libtasn1/no-benchmark.patch
endif
ifdef HAVE_VISUALSTUDIO
	$(APPLY) $(SRC)/libtasn1/msvc.patch
endif
	$(MOVE)

LIBTASN1_CFLAGS := $(CFLAGS) -DASN1_STATIC

.libtasn1: libtasn1
	cd $< && $(HOSTVARS) CFLAGS="$(LIBTASN1_CFLAGS)" ./configure $(HOSTCONF) --disable-doc
	cd $< && $(MAKE) install
	touch $@
