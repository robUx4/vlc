# Game Music Emu

GME_VERSION := 0.6.0
GME_URL := https://bitbucket.org/mpyne/game-music-emu/downloads/game-music-emu-$(GME_VERSION).tar.bz2

PKGS += gme

$(TARBALLS)/game-music-emu-$(GME_VERSION).tar.bz2:
	$(call download,$(GME_URL))

.sum-gme: game-music-emu-$(GME_VERSION).tar.bz2

game-music-emu: game-music-emu-$(GME_VERSION).tar.bz2 .sum-gme
	$(UNPACK)
	$(APPLY) $(SRC)/gme/gme-static.patch
	$(APPLY) $(SRC)/gme/gme-quotes.patch
	$(APPLY) $(SRC)/gme/skip-underrun.patch
	$(MOVE)

.gme: game-music-emu toolchain.cmake
	cd $< && $(HOSTVARS_CMAKE) $(CMAKE) .
ifdef HAVE_VISUALSTUDIO
	cd $< && msbuild.exe -p:VisualStudioVersion=$(MSBUILD_COMPILER) -p:Configuration=$(VLC_CONFIGURATION) -m -nologo INSTALL.vcxproj
	mkdir -p -- "$(PREFIX)/lib"
else
	cd $< && $(MAKE) install
endif
	touch $@
