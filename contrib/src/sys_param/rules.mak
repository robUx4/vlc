# generate Direct3D11 temporary include

ifdef HAVE_CROSS_COMPILE
IDL_INC_PATH = /usr/include/wine/windows/
else
#ugly way to get the default location of standard idl files
IDL_INC_PATH = /`echo $(MSYSTEM) | tr A-Z a-z`/$(BUILD)/include
endif

DST_SYS_PARAM_H = $(PREFIX)/include/sys/param.h

ifdef HAVE_WINSTORE
PKGS += sys_param
endif

$(DST_SYS_PARAM_H): $(SRC)/sys_param/param.h
	mkdir -p $(PREFIX)/include/sys
	cp $< $@

.sys_param: $(DST_SYS_PARAM_H)
	touch $@
