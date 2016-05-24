# GCRYPT
GCRYPT_VERSION := 1.6.4
GCRYPT_URL := ftp://ftp.gnupg.org/gcrypt/libgcrypt/libgcrypt-$(GCRYPT_VERSION).tar.bz2

PKGS += gcrypt

$(TARBALLS)/libgcrypt-$(GCRYPT_VERSION).tar.bz2:
	$(call download,$(GCRYPT_URL))

.sum-gcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2

libgcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2 .sum-gcrypt
	$(UNPACK)
	$(APPLY) $(SRC)/gcrypt/fix-amd64-assembly-on-solaris.patch
	$(APPLY) $(SRC)/gcrypt/0001-Fix-assembly-division-check.patch
	$(APPLY) $(SRC)/gcrypt/disable-doc-compilation.patch
	$(APPLY) $(SRC)/gcrypt/disable-tests-compilation.patch
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/gcrypt/winrt.patch
endif
ifdef HAVE_VISUALSTUDIO
	$(APPLY) $(SRC)/gcrypt/alloca.patch
	$(APPLY) $(SRC)/gcrypt/msvc.patch
endif
	$(MOVE)

DEPS_gcrypt = gpg-error

GCRYPT_CONF = \
	--enable-ciphers=aes,des,rfc2268,arcfour \
	--enable-digests=sha1,md5,rmd160,sha256,sha512 \
	--enable-pubkey-ciphers=dsa,rsa,ecc

ifdef HAVE_WIN64
GCRYPT_CONF += --disable-asm --disable-padlock-support
endif
ifdef HAVE_VISUALSTUDIO
GCRYPT_CONF += --disable-asm
endif
ifdef HAVE_IOS
GCRYPT_EXTRA_CFLAGS = -fheinous-gnu-extensions
else
GCRYPT_EXTRA_CFLAGS =
endif
ifdef HAVE_MACOSX
GCRYPT_CONF += --disable-aesni-support
else
ifdef HAVE_BSD
GCRYPT_CONF += --disable-asm --disable-aesni-support
endif
endif
ifdef HAVE_ANDROID
ifeq ($(ANDROID_ABI), x86)
GCRYPT_CONF += ac_cv_sys_symbol_underscore=no
endif
ifeq ($(ANDROID_ABI), x86_64)
GCRYPT_CONF += ac_cv_sys_symbol_underscore=no
endif
endif
ifdef HAVE_TIZEN
ifeq ($(TIZEN_ABI), x86)
GCRYPT_CONF += ac_cv_sys_symbol_underscore=no
endif
endif

.gcrypt: libgcrypt
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) $(GCRYPT_EXTRA_CFLAGS)" ./configure $(HOSTCONF) $(GCRYPT_CONF)
	cd $< && $(MAKE) install
	touch $@
