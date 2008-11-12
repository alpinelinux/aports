# Makefile - one file to rule them all, one file to bind them
#
# Copyright (C) 2007 Timo Teräs <timo.teras@iki.fi>
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it 
# under the terms of the GNU General Public License version 3 as published
# by the Free Software Foundation. See http://www.gnu.org/ for details.

VERSION := 2.0_pre1

SVN_REV := $(shell svn info 2> /dev/null | grep ^Revision | cut -d ' ' -f 2)
ifneq ($(SVN_REV),)
FULL_VERSION := $(VERSION)-r$(SVN_REV)
else
FULL_VERSION := $(VERSION)
endif

CC=gcc
INSTALL=install
INSTALLDIR=$(INSTALL) -d

CFLAGS=-g -D_GNU_SOURCE -Werror -Wall -Wstrict-prototypes -std=gnu99 \
	-DAPK_VERSION=\"$(FULL_VERSION)\"
LDFLAGS=-g
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

.PHONY: compile install clean all

all: compile

compile install clean::
	@for i in $(SUBDIRS); do $(MAKE) $(MFLAGS) -C $$i $(MAKECMDGOALS); done

install::
	$(INSTALLDIR) $(DESTDIR)$(DOCDIR)
	$(INSTALL) README $(DESTDIR)$(DOCDIR)

dist:
	svn-clean
	(TOP=`pwd` && cd .. && ln -s $$TOP apk-tools-$(VERSION) && \
	 tar --exclude '*/.svn*' -cjvf apk-tools-$(VERSION).tar.bz2 apk-tools-$(VERSION)/* && \
	 rm apk-tools-$(VERSION))

.EXPORT_ALL_VARIABLES:
