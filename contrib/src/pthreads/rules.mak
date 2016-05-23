# POSIX threads

ifndef HAVE_WIN32
PKGS_FOUND += pthreads
endif

PTHREADS_W32_VERSION := 2-9-1
PTHREADS_W32_URL := ftp://sources.redhat.com/pub/pthreads-win32/pthreads-w32-$(PTHREADS_W32_VERSION)-release.tar.gz

$(TARBALLS)/pthreads-w32-$(PTHREADS_W32_VERSION)-release.tar.gz:
	$(call download_pkg,$(PTHREADS_W32_URL),pthreads)

.sum-pthreads: pthreads-w32-$(PTHREADS_W32_VERSION)-release.tar.gz

ifdef HAVE_WIN32
pthreads: pthreads-w32-$(PTHREADS_W32_VERSION)-release.tar.gz .sum-pthreads
	$(UNPACK)
	sed -e 's/^CROSS.*=/CROSS ?=/' -i.orig $(UNPACK_DIR)/GNUmakefile
	sed -e 's/^CFLAGS.*=/CFLAGS +=/' -i.orig $(UNPACK_DIR)/GNUmakefile
	sed -e 's/RCFLAGS		=/RCFLAGS		+=/' -i.orig $(UNPACK_DIR)/GNUmakefile
	sed -i.orig \
		-e 's/AR      =/#AR      =/' \
		-e 's/CC      =/#CC      =/' \
		-e 's/CXX     =/#CXX     =/' \
		-e 's/RANLIB  =/#RANLIB  =/' \
		$(UNPACK_DIR)/GNUmakefile
	$(APPLY) $(SRC)/pthreads/arm-dll.patch
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/pthreads/winrt.patch
endif
	$(MOVE)

ifdef HAVE_CROSS_COMPILE
ifndef HAVE_VISUALSTUDIO
PTHREADS_W32_CONF := CROSS="$(HOST)-"
endif
endif

PTHREAD_CFLAGS =
PTHREAD_RCFLAGS =
ifeq ($(ARCH),arm)
PTHREAD_RCFLAGS += -DPTW32_ARCHarm
endif
ifeq ($(ARCH),x86_64)
PTHREAD_RCFLAGS += -DPTW32_ARCHx64
endif
ifeq ($(ARCH),i386)
PTHREAD_RCFLAGS += -DPTW32_ARCHx86
endif
ifdef HAVE_WIN32
PTHREAD_CFLAGS += -DPTW32_RC_MSC
PTHREAD_RCFLAGS += -DPTW32_RC_MSC
endif

.pthreads: pthreads
	cd $< && $(HOSTVARS) $(PTHREADS_W32_CONF) CFLAGS="$(CFLAGS) $(PTHREAD_CFLAGS)" RCFLAGS="$(RCFLAGS) $(PTHREAD_RCFLAGS)" $(MAKE) MAKEFLAGS=-j1 GC-static
	mkdir -p -- "$(PREFIX)/include"
	cd $< && cp -v pthread.h sched.h semaphore.h "$(PREFIX)/include/"
	sed -e 's/#if HAVE_CONFIG_H/#if 0 \&\& HAVE_CONFIG_H/' -i \
		"$(PREFIX)/include/pthread.h"
	mkdir -p -- "$(PREFIX)/lib"
ifndef HAVE_VISUALSTUDIO
	cp -v $</*.a $</*.dll "$(PREFIX)/lib/"
else
	cp -v $</libpthreadGC2.a "$(PREFIX)/lib/pthreadGC2.lib"
endif
	touch $@
endif
