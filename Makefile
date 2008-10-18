
PACKAGE=abuild
VERSION=0.3
DISTFILES=Makefile abuild abuild.conf

prefix ?= /usr
sysconfdir ?= /etc

P=$(PACKAGE)-$(VERSION)

help:
	@echo "$(P) makefile"
	@echo "usage: make install [ DESTDIR=<path> ]"
	@echo "       make dist"

install:
	mkdir -p $(DESTDIR)/$(prefix)/bin $(DESTDIR)/$(sysconfdir)
	cp abuild $(DESTDIR)/$(prefix)/bin/
	cp abuild.conf $(DESTDIR)/$(sysconfdir)/

dist:	$(P).tar.gz

$(P).tar.gz:	$(DISTFILES)
	rm -rf $(P)
	mkdir -p $(P)
	cp $(DISTFILES) $(P)/
	tar -czf $@ $(P)
	rm -rf $(P)

.PHONY: install dist
