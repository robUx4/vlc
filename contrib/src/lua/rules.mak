# Lua 5.3

LUA_VERSION := 5.3.2
LUA_VERSION_MAJOR := 5.3
LUA_URL := http://www.lua.org/ftp/lua-$(LUA_VERSION).tar.gz

# Reverse priority order
LUA_TARGET := generic
ifdef HAVE_BSD
LUA_TARGET := bsd
endif
ifdef HAVE_LINUX
LUA_TARGET := linux
endif
ifdef HAVE_MACOSX
LUA_TARGET := macosx
endif
ifdef HAVE_IOS
LUA_TARGET := ios
endif
ifdef HAVE_WIN32
LUA_TARGET := mingw
endif
ifdef HAVE_SOLARIS
LUA_TARGET := solaris
endif

# Feel free to add autodetection if you need to...
PKGS += lua
ifeq ($(call need_pkg,"lua5.3"),)
PKGS_FOUND += lua
endif
ifeq ($(call need_pkg,"lua5.2"),)
PKGS_FOUND += lua
endif
ifeq ($(call need_pkg,"lua5.1"),)
PKGS_FOUND += lua
endif

$(TARBALLS)/lua-$(LUA_VERSION).tar.gz:
	$(call download,$(LUA_URL))

.sum-lua: lua-$(LUA_VERSION).tar.gz

lua: lua-$(LUA_VERSION).tar.gz .sum-lua
	$(UNPACK)
	$(APPLY) $(SRC)/lua/lua-noreadline.patch
	$(APPLY) $(SRC)/lua/no-dylibs.patch
	$(APPLY) $(SRC)/lua/no-localeconv.patch
	$(APPLY) $(SRC)/lua/lua-android-log2.patch
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/lua/winrt-nopopen.patch
	$(APPLY) $(SRC)/lua/winrt-nosystem.patch
endif
ifdef HAVE_VISUALSTUDIO
	$(APPLY) $(SRC)/lua/msvc.patch
endif
ifdef HAVE_DARWIN_OS
	(cd $(UNPACK_DIR) && \
	sed -e 's%gcc%$(CC)%' \
		-e 's%LDFLAGS=%LDFLAGS=$(EXTRA_CFLAGS) $(EXTRA_LDFLAGS)%' \
		-i.orig src/Makefile)
endif
ifdef HAVE_SOLARIS
	(cd $(UNPACK_DIR) && \
	sed -e 's%LIBS="-ldl"$$%LIBS="-ldl" MYLDFLAGS="$(EXTRA_LDFLAGS)"%' \
		-i.orig src/Makefile)
endif
ifdef HAVE_WIN32
	cd $(UNPACK_DIR) && sed -i.orig -e 's/lua luac/lua.exe luac.exe/' Makefile
endif
	cd $(UNPACK_DIR)/src && sed -i.orig \
		-e 's/CC=/#CC=/' \
		-e 's/= *strip/=$(STRIP)/' \
		-e 's/= *ranlib/= $(RANLIB)/' \
		Makefile
	$(MOVE)

LUAVARS=$(HOSTVARS) MYCFLAGS="$(CFLAGS)" MYLDFLAGS="$(LDFLAGS)"
LUAVARS_PIC=$(HOSTVARS_PIC) MYCFLAGS="$(CFLAGS)" MYLDFLAGS="$(LDFLAGS)"

.lua: lua
	cd $< && $(LUAVARS_PIC) $(MAKE) $(LUA_TARGET)
ifdef HAVE_WIN32
	cd $< && $(LUAVARS) $(MAKE) -C src liblua.a
endif
	cd $< && $(LUAVARS) $(MAKE) install INSTALL_TOP="$(PREFIX)"
	mkdir -p -- "$(PREFIX)/lib/pkgconfig"
	sed -e 's/@VERSION_MAJOR@/$(LUA_VERSION_MAJOR)/g' -e 's/@VERSION@/$(LUA_VERSION)/g' <  $(SRC)/lua/lua.pc.in > "$(PREFIX)/lib/pkgconfig/lua.pc"
ifdef HAVE_WIN32
	cd $< && $(RANLIB) "$(PREFIX)/lib/liblua.a"
endif
ifdef HAVE_VISUALSTUDIO
	cp $</src/liblua.a "$(PREFIX)/lib/lua.lib"
endif
ifndef HAVE_VISUALSTUDIO
ifdef HAVE_CROSS_COMPILE
	cd $</src && $(MAKE) clean && $(MAKE) liblua.a && ranlib liblua.a && $(MAKE) luac
	cp $</src/luac $(PREFIX)/bin
endif
endif
	touch $@
