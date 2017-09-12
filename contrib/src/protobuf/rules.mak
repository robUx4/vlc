# protobuf
PROTOBUF_VERSION := 3.1.0
PROTOBUF_URL := https://github.com/google/protobuf/releases/download/v$(PROTOBUF_VERSION)/protobuf-cpp-$(PROTOBUF_VERSION).tar.gz

PKGS += protoc protobuf
PKGS_ALL += protoc
ifeq ($(call need_pkg,"protobuf"),)
PKGS_FOUND += protobuf
endif

$(TARBALLS)/protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz:
	$(call download_pkg,$(PROTOBUF_URL),protobuf)

.sum-protobuf: protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz

.sum-protoc: .sum-protobuf
	touch $@

DEPS_protobuf = zlib $(DEPS_zlib)

protobuf: protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz .sum-protobuf
	$(UNPACK)
	mv protobuf-3.1.0 protobuf-3.1.0-cpp
	$(APPLY) $(SRC)/protobuf/dont-build-protoc.patch
	$(MOVE)

.protobuf: protobuf
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --with-protoc="$(PROTOC)"
	cd $< && $(MAKE) && $(MAKE) install
	touch $@

protoc: protobuf-$(PROTOBUF_VERSION).tar.gz .sum-protoc
	# DO NOT use the same intermediate directory as the protobuf target
	rm -Rf -- protoc
	mkdir -- protoc
	tar -x -v -z -C protoc --strip-components=1 -f $<
ifdef HAVE_WIN32
	(cd protoc && patch -p1) < $(SRC)/protobuf/win32.patch
endif

.protoc: protoc
	$(RECONF)
	cd $< && $(MAKE)
	mkdir -p -- $(BUILDBINDIR)
	install -m 0755 -s -- $</src/protoc $(BUILDBINDIR)/$(HOST)-protoc
	touch $@
