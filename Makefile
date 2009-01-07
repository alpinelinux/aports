
PACKAGE=abuild
VERSION:=$(shell  awk -F= '$$1 == "abuild_ver" {print $$2}' abuild)
DISTFILES=Makefile abuild abuild.conf APKBUILD.proto buildrepo

prefix ?= /usr
sysconfdir ?= /etc
datadir ?= $(prefix)/share/$(PACKAGE)

P=$(PACKAGE)-$(VERSION)

help:
	@echo "$(P) makefile"
	@echo "usage: make install [ DESTDIR=<path> ]"
	@echo "       make dist"

install: abuild abuild.conf APKBUILD.proto functions.sh
	mkdir -p $(DESTDIR)/$(prefix)/bin $(DESTDIR)/$(sysconfdir) \
		$(DESTDIR)/$(datadir)
	cp abuild buildrepo $(DESTDIR)/$(prefix)/bin/
	if [ -z "$(DESTDIR)" ] && [ ! -f "/$(sysconfdir)"/abuild.conf ]; then\
		cp abuild.conf $(DESTDIR)/$(sysconfdir)/; \
	fi
	cp APKBUILD.proto $(DESTDIR)/$(prefix)/share/abuild
	cp functions.sh $(DESTDIR)/$(datadir)/

dist:	$(P).tar.gz

$(P).tar.gz:	$(DISTFILES)
	rm -rf $(P)
	mkdir -p $(P)
	cp $(DISTFILES) $(P)/
	tar -czf $@ $(P)
	rm -rf $(P)

.PHONY: install dist
