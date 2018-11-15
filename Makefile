##
# Building apk-tools

-include config.mk

PACKAGE := apk-tools
VERSION := 2.10.3

##
# Default directories

DESTDIR		:=
SBINDIR		:= /sbin
LIBDIR		:= /lib
CONFDIR		:= /etc/apk
MANDIR		:= /usr/share/man
DOCDIR		:= /usr/share/doc/apk

export DESTDIR SBINDIR LIBDIR CONFDIR MANDIR DOCDIR

##
# Top-level rules and targets

targets		:= libfetch/ src/

##
# Include all rules and stuff

include Make.rules

##
# Top-level targets

install:
	$(INSTALLDIR) $(DESTDIR)$(DOCDIR)
	$(INSTALL) README $(DESTDIR)$(DOCDIR)

check test: FORCE
	$(Q)$(MAKE) TEST=y
	$(Q)$(MAKE) -C test

static:
	$(Q)$(MAKE) STATIC=y

tag: check
	git commit . -m "apk-tools-$(VERSION)"
	git tag -s v$(VERSION) -m "apk-tools-$(VERSION)"

src/: libfetch/
