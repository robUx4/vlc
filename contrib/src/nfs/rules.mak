# NFS
NFS_VERSION := 1.10.0
NFS_URL := https://github.com/sahlberg/libnfs/archive/libnfs-$(NFS_VERSION).tar.gz

PKGS += nfs
ifeq ($(call need_pkg,"libnfs"),)
PKGS_FOUND += nfs
endif

$(TARBALLS)/libnfs-$(NFS_VERSION).tar.gz:
	$(call download,$(NFS_URL))

.sum-nfs: libnfs-$(NFS_VERSION).tar.gz

nfs: libnfs-$(NFS_VERSION).tar.gz .sum-nfs
	$(UNPACK)
	mv libnfs-libnfs-$(NFS_VERSION) libnfs-$(NFS_VERSION)
	$(APPLY) $(SRC)/nfs/Android-statvfs.patch
ifdef HAVE_VISUALSTUDIO
	$(APPLY) $(SRC)/nfs/non-gcc.patch
	$(APPLY) $(SRC)/nfs/msvc.patch
endif
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.nfs: nfs
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure --disable-examples --disable-utils $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
