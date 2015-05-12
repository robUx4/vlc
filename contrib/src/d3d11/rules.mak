# Direct3D11 temporary include

$(TOPDST)/$(HOST)/include/d3d11.h:
	cp $(SRC)/d3d11/d3d11.h $@

.d3d11: $(TOPDST)/$(HOST)/include/d3d11.h
	touch $@
