# Makefile - one file to rule them all, one file to bind them
#
# Copyright (C) 2007 Timo Teräs <timo.teras@iki.fi>
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it 
# under the terms of the GNU General Public License version 3 as published
# by the Free Software Foundation. See http://www.gnu.org/ for details.

all:

clean:

install::
	$(INSTALLDIR) $(DESTDIR)$(SBINDIR)
	$(INSTALL) apk_create $(DESTDIR)$(SBINDIR)
	$(INSTALLDIR) $(DESTDIR)/lib/apk
	$(INSTALL) libutil.sh $(DESTDIR)/lib/apk

