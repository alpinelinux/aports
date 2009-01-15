
PACKAGE=abuild
VERSION:=$(shell  awk -F= '$$1 == "abuild_ver" {print $$2}' abuild)
USR_BIN_FILES=abuild devbuild mkalpine buildrepo
DISTFILES=$(USR_BIN_FILES) Makefile abuild.conf APKBUILD.proto 


prefix ?= /usr
sysconfdir ?= /etc
datadir ?= $(prefix)/share/$(PACKAGE)

P=$(PACKAGE)-$(VERSION)

help:
	@echo "$(P) makefile"
	@echo "usage: make install [ DESTDIR=<path> ]"
	@echo "       make dist"

install: $(USR_BIN_FILES) abuild.conf APKBUILD.proto functions.sh
	mkdir -p $(DESTDIR)/$(prefix)/bin $(DESTDIR)/$(sysconfdir) \
		$(DESTDIR)/$(datadir)
	for i in $(USR_BIN_FILES); do\
		install -m 755 $$i $(DESTDIR)/$(prefix)/bin/$$i;\
	done
	if [ -z "$(DESTDIR)" ] && [ ! -f "/$(sysconfdir)"/abuild.conf ]; then\
		cp abuild.conf $(DESTDIR)/$(sysconfdir)/; \
	fi
	cp APKBUILD.proto $(DESTDIR)/$(prefix)/share/abuild
	cp functions.sh $(DESTDIR)/$(datadir)/

dist:	$(P).tar.bz2

$(P).tar.bz2:	$(DISTFILES)
	rm -rf $(P)
	mkdir -p $(P)
	cp $(DISTFILES) $(P)/
	tar -cjf $@ $(P)
	rm -rf $(P)

.PHONY: install dist
