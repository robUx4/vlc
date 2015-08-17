# ebml

EBML_VERSION := 1.3.1
EBML_URL := http://dl.matroska.org/downloads/libebml/libebml-$(EBML_VERSION).tar.bz2
#EBML_URL := $(CONTRIB_VIDEOLAN)/libebml-$(EBML_VERSION).tar.bz2

$(TARBALLS)/libebml-$(EBML_VERSION).tar.bz2:
	$(call download,$(EBML_URL))

.sum-ebml: libebml-$(EBML_VERSION).tar.bz2

libebml: libebml-$(EBML_VERSION).tar.bz2 .sum-ebml
	$(UNPACK)
ifdef HAVE_WINRT
	$(APPLY) $(SRC)/ebml/winstore.patch
endif
	$(MOVE)

# libebml requires exceptions
EBML_EXTRA_FLAGS = CXXFLAGS="${CXXFLAGS} -fexceptions -fvisibility=hidden" \
					CPPFLAGS=""

.ebml: libebml
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(EBML_EXTRA_FLAGS)
	cd $< && $(MAKE) install
ifdef HAVE_VISUALSTUDIO
	cp $(PREFIX)/lib/ebml.lib $(PREFIX)/lib/libebml.a
endif
	touch $@
