
PACKAGE		:= abuild
VERSION		:= 2.13.0

prefix		?= /usr
bindir		?= $(prefix)/bin
sysconfdir	?= /etc
datadir		?= $(prefix)/share/$(PACKAGE)
abuildrepo	?= ~/.cache/abuild

LUA_VERSION	= 5.1
LUA_SHAREDIR	?= $(prefix)/share/lua/$(LUA_VERSION)/

SCRIPTS		:= abuild buildrepo abuild-keygen abuild-sign newapkbuild \
		   abump apkgrel ap buildlab apkbuild-cpan
USR_BIN_FILES	:= $(SCRIPTS) abuild-tar abuild-sudo
SAMPLES		:= sample.APKBUILD sample.initd sample.confd \
		sample.pre-install sample.post-install

SCRIPT_SOURCES	:= $(addsuffix .in,$(SCRIPTS))

DISTFILES=$(SCRIPT_SOURCES) $(SAMPLES) Makefile abuild.conf 

GIT_REV		:= $(shell test -d .git && git describe || echo exported)
ifneq ($(GIT_REV), exported)
FULL_VERSION    := $(patsubst $(PACKAGE)-%,%,$(GIT_REV))
FULL_VERSION    := $(patsubst v%,%,$(FULL_VERSION))
else
FULL_VERSION    := $(VERSION)
endif

CHMOD		:= chmod
SED		:= sed
TAR		:= tar
LINK		= $(CC) $(OBJS-$@) -o $@ $(LDFLAGS) $(LDFLAGS-$@) $(LIBS-$@)

SED_REPLACE	:= -e 's:@VERSION@:$(FULL_VERSION):g' \
			-e 's:@prefix@:$(prefix):g' \
			-e 's:@sysconfdir@:$(sysconfdir):g' \
			-e 's:@datadir@:$(datadir):g' \
			-e 's:@abuildrepo@:$(abuildrepo):g'

SSL_CFLAGS	= $(shell pkg-config --cflags openssl)
SSL_LIBS	= $(shell pkg-config --libs openssl)

LDFLAGS ?= 

OBJS-abuild-tar  = abuild-tar.o
LIBS-abuild-tar = $(SSL_LIBS)
CFLAGS-abuild-tar = $(SSL_CFLAGS)

OBJS-abuild-sudo = abuild-sudo.o

.SUFFIXES:	.sh.in .in
%.sh: %.sh.in
	${SED} ${SED_REPLACE} ${SED_EXTRA} $< > $@
	${CHMOD} +x $@

%: %.in
	${SED} ${SED_REPLACE} ${SED_EXTRA} $< > $@
	${CHMOD} +x $@

P=$(PACKAGE)-$(VERSION)

all: 	$(USR_BIN_FILES)

clean:
	@rm -f $(USR_BIN_FILES)

%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS-$@) -o $@ -c $<

abuild-sudo: abuild-sudo.o
	$(LINK)

abuild-tar: abuild-tar.o
	$(LINK)

abuild-tar.static: abuild-tar.o
	$(CC) $(CFLAGS) $(CFLAGS-$@) -o $@ -static $(LIBS-$@) $^

help:
	@echo "$(P) makefile"
	@echo "usage: make install [ DESTDIR=<path> ]"
	@echo "       make dist"

install: $(USR_BIN_FILES) $(SAMPLES) abuild.conf functions.sh aports.lua
	install -d $(DESTDIR)/$(bindir) $(DESTDIR)/$(sysconfdir) \
		$(DESTDIR)/$(datadir)
	for i in $(USR_BIN_FILES); do\
		install -m 755 $$i $(DESTDIR)/$(bindir)/$$i;\
	done
	chmod 4111 $(DESTDIR)/$(prefix)/bin/abuild-sudo
	for i in adduser addgroup apk; do \
		ln -fs abuild-sudo $(DESTDIR)/$(bindir)/abuild-$$i; \
	done
	if [ -n "$(DESTDIR)" ] || [ ! -f "/$(sysconfdir)"/abuild.conf ]; then\
		cp abuild.conf $(DESTDIR)/$(sysconfdir)/; \
	fi
	cp $(SAMPLES) $(DESTDIR)/$(prefix)/share/abuild
	cp functions.sh $(DESTDIR)/$(datadir)/
	mkdir -p $(DESTDIR)$(LUA_SHAREDIR)
	cp aports.lua $(DESTDIR)$(LUA_SHAREDIR)/

dist:	$(P).tar.bz2

$(P).tar.bz2:	$(DISTFILES)
	rm -rf $(P)
	mkdir -p $(P)
	cp $(DISTFILES) $(P)/
	tar -cjf $@ $(P)
	rm -rf $(P)

.gitignore: Makefile
	echo "*.tar.bz2" > $@
	for i in $(USR_BIN_FILES); do\
		echo $$i >>$@;\
	done


.PHONY: install dist
