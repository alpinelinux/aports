
PACKAGE=abuild
VERSION=0.3
DISTFILES=Makefile abuild abuild.conf APKBUILD.proto

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
	cp abuild $(DESTDIR)/$(prefix)/bin/
	cp abuild.conf $(DESTDIR)/$(sysconfdir)/
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
