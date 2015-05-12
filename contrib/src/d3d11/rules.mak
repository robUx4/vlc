# Direct3D11 temporary include

DST_FILE = $(TOPDST)/$(HOST)/include/d3d11.h

$(DST_FILE):
	-cp $(SRC)/d3d11/d3d11.h $@

.d3d11: $(DST_FILE)
	-touch $@
