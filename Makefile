# Makefile - one file to rule them all, one file to bind them
#
# Copyright (C) 2007 Timo Teräs <timo.teras@iki.fi>
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it 
# under the terms of the GNU General Public License version 3 as published
# by the Free Software Foundation. See http://www.gnu.org/ for details.

PACKAGE := apk-tools
VERSION := 2.0_pre2

SVN_REV := $(shell svn info 2> /dev/null | grep ^Revision | cut -d ' ' -f 2)
ifneq ($(SVN_REV),)
FULL_VERSION := $(VERSION)-r$(SVN_REV)
else
FULL_VERSION := $(VERSION)
endif

CC=gcc
INSTALL=install
INSTALLDIR=$(INSTALL) -d

CFLAGS?=-g -Werror -Wall -Wstrict-prototypes
CFLAGS+=-D_GNU_SOURCE -std=gnu99 -DAPK_VERSION=\"$(FULL_VERSION)\"

LDFLAGS?=-g
LDFLAGS+=-nopie
LIBS=/usr/lib/libz.a

ifeq ($(STATIC),yes)
CFLAGS+=-fno-stack-protector
LDFLAGS+=-static
endif

DESTDIR=
SBINDIR=/sbin
CONFDIR=/etc/apk
MANDIR=/usr/share/man
DOCDIR=/usr/share/doc/apk

SUBDIRS=src

.PHONY: compile install clean all static

all: compile

static:
	$(MAKE) $(MFLAGS) -C src apk.static

compile install clean::
	@for i in $(SUBDIRS); do $(MAKE) $(MFLAGS) -C $$i $(MAKECMDGOALS); done

install::
	$(INSTALLDIR) $(DESTDIR)$(DOCDIR)
	$(INSTALL) README $(DESTDIR)$(DOCDIR)

clean::
	rm -rf $(TARBALL)

TARBALL := $(PACKAGE)-$(VERSION).tar.bz2
dist:	$(TARBALL)
$(TARBALL):
	rm -rf $(PACKAGE)
	git clone . $(PACKAGE)
	cd $(PACKAGE) && (cd .. && git diff) | patch -p1
	tar -cjf $@ $(PACKAGE)
	rm -rf $(PACKAGE)

.EXPORT_ALL_VARIABLES:
