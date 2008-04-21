# Makefile - one file to rule them all, one file to bind them
#
# Copyright (C) 2007 Timo Teräs <timo.teras@iki.fi>
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it 
# under the terms of the GNU General Public License version 3 as published
# by the Free Software Foundation. See http://www.gnu.org/ for details.

TARGETS = apk

apk_OBJS = \
	state.o \
	database.o \
	package.o \
	archive.o \
	version.o \
	blob.o \
	hash.o \
	md5.o \
	create.o \
	add.o \
	del.o \
	ver.o \
	index.o \
	apk.o

ALL_OBJS = $(apk_OBJS)

all: $(TARGETS)

apk: $(apk_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean::
	@rm -f $(TARGETS) $(ALL_OBJS)

install::
	$(INSTALLDIR) $(DESTDIR)$(SBINDIR)
	$(INSTALL) $(TARGETS) $(DESTDIR)$(SBINDIR)

