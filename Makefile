##
# Building apk-tools

-include config.mk

PACKAGE := apk-tools
VERSION := 2.1.0_rc1

##
# Default directories

DESTDIR		:=
SBINDIR		:= /sbin
LIBDIR		:= /lib
CONFDIR		:= /etc/apk
MANDIR		:= /usr/share/man
DOCDIR		:= /usr/share/doc/apk
LUA_LIBDIR	:= /usr/lib/lua/5.1

export DESTDIR SBINDIR LIBDIR CONFDIR MANDIR DOCDIR LUA_LIBDIR

##
# Top-level rules and targets

targets		:= src/

##
# Include all rules and stuff

include Make.rules

##
# Top-level targets

install:
	$(INSTALLDIR) $(DESTDIR)$(DOCDIR)
	$(INSTALL) README $(DESTDIR)$(DOCDIR)

static:
	$(Q)$(MAKE) STATIC=y
