# mfx (Media SDK)

mfx_GITURL := https://github.com/lu-zero/mfx_dispatch.git
MFX_GITHASH := 5d6f1b698045e2a20d89165c8be17d28a0782cbe

ifeq ($(call need_pkg,"mfx"),)
PKGS_FOUND += mfx
endif
ifdef HAVE_WIN32
PKGS += mfx
endif

$(TARBALLS)/mfx-$(MFX_GITHASH).tar.xz:
	$(call download_git,$(mfx_GITURL),,$(MFX_GITHASH))

.sum-mfx: mfx-$(MFX_GITHASH).tar.xz
	$(call check_githash,$(MFX_GITHASH))
	touch $@

mfx: mfx-$(MFX_GITHASH).tar.xz .sum-mfx
	$(UNPACK)
	cd $(UNPACK_DIR) && autoreconf -ivf
	$(MOVE)

.mfx: mfx
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
