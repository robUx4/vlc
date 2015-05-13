# generate Direct3D11 temporary include

#ugly way to get the default location of standard idl files
IDL_INC_PATH = /`echo $(MSYSTEM) | tr A-Z a-z`/$(BUILD)/include

D3D11_IDL_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/master/tree/mingw-w64-headers/direct-x/include/d3d11.idl?format=raw
DST_D3D11_H = $(PREFIX)/include/d3d11.h
DST_DXGIDEBUG_H = $(PREFIX)/include/dxgidebug.h
UNPACK_DIR = $(TARBALLS)


ifdef HAVE_WIN32
PKGS += d3d11
endif

$(TARBALLS)/d3d11.idl:
	$(call download,$(D3D11_IDL_URL))
	$(APPLY) $(SRC)/d3d11/id3d11videodecoder.patch

$(TARBALLS)/dxgidebug.idl:
	$(APPLY) $(SRC)/d3d11/dxgidebug.patch

$(DST_D3D11_H): $(TARBALLS)/d3d11.idl
	widl -DBOOL=WINBOOL -I$(IDL_INC_PATH) -h -o $@ $<

$(DST_DXGIDEBUG_H): $(TARBALLS)/dxgidebug.idl
	widl -DBOOL=WINBOOL -I$(IDL_INC_PATH) -h -o $@ $<

.d3d11: $(DST_D3D11_H) $(DST_DXGIDEBUG_H)
	touch $@
