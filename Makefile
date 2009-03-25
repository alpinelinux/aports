
PACKAGE=abuild
VERSION:=$(shell  awk -F= '$$1 == "abuild_ver" {print $$2}' abuild)
USR_BIN_FILES=abuild devbuild mkalpine buildrepo
SAMPLES=sample.APKBUILD sample.initd sample.confd sample.pre-install \
	sample.post-install
DISTFILES=$(USR_BIN_FILES) $(SAMPLES) Makefile abuild.conf initramfs-init \


prefix ?= /usr
sysconfdir ?= /etc
datadir ?= $(prefix)/share/$(PACKAGE)

P=$(PACKAGE)-$(VERSION)

help:
	@echo "$(P) makefile"
	@echo "usage: make install [ DESTDIR=<path> ]"
	@echo "       make dist"

install: $(USR_BIN_FILES) $(SAMPLES) abuild.conf functions.sh
	mkdir -p $(DESTDIR)/$(prefix)/bin $(DESTDIR)/$(sysconfdir) \
		$(DESTDIR)/$(datadir)
	for i in $(USR_BIN_FILES); do\
		install -m 755 $$i $(DESTDIR)/$(prefix)/bin/$$i;\
	done
	if [ -n "$(DESTDIR)" ] || [ ! -f "/$(sysconfdir)"/abuild.conf ]; then\
		cp abuild.conf $(DESTDIR)/$(sysconfdir)/; \
	fi
	cp $(SAMPLES) $(DESTDIR)/$(prefix)/share/abuild
	cp functions.sh $(DESTDIR)/$(datadir)/
	cp initamfs-init $(DESTDIR)/$(datadir)/

dist:	$(P).tar.bz2

$(P).tar.bz2:	$(DISTFILES)
	rm -rf $(P)
	mkdir -p $(P)
	cp $(DISTFILES) $(P)/
	tar -cjf $@ $(P)
	rm -rf $(P)

.PHONY: install dist
