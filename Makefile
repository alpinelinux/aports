
PACKAGE		:= abuild
VERSION		:= 3.3.0_pre1

prefix		?= /usr
bindir		?= $(prefix)/bin
sysconfdir	?= /etc
datadir		?= $(prefix)/share/$(PACKAGE)
mandir		?= $(prefix)/share/man

SCRIPTS		:= abuild abuild-keygen abuild-sign newapkbuild \
		   abump apkgrel buildlab apkbuild-cpan checkapk \
		   apkbuild-gem-resolver
USR_BIN_FILES	:= $(SCRIPTS) abuild-tar abuild-gzsplit abuild-sudo abuild-fetch abuild-rmtemp
MAN_1_PAGES	:= newapkbuild.1
MAN_5_PAGES	:= APKBUILD.5
SAMPLES		:= sample.APKBUILD sample.initd sample.confd \
		sample.pre-install sample.post-install
AUTOTOOLS_TOOLCHAIN_FILES := config.sub config.guess

SCRIPT_SOURCES	:= $(addsuffix .in,$(SCRIPTS))

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

SSL_CFLAGS	?= $(shell pkg-config --cflags openssl)
SSL_LDFLAGS	?= $(shell pkg-config --cflags openssl)
SSL_LIBS	?= $(shell pkg-config --libs openssl)
ZLIB_LIBS	?= $(shell pkg-config --libs zlib)

OBJS-abuild-tar  = abuild-tar.o
CFLAGS-abuild-tar.o = $(SSL_CFLAGS)
LDFLAGS-abuild-tar = $(SSL_LDFLAGS)
LIBS-abuild-tar = $(SSL_LIBS)

OBJS-abuild-gzsplit = abuild-gzsplit.o
LDFLAGS-abuild-gzsplit = $(ZLIB_LIBS)

OBJS-abuild-sudo = abuild-sudo.o
OBJS-abuild-fetch = abuild-fetch.o

.SUFFIXES:	.sh.in .in
%.sh: %.sh.in
	${SED} ${SED_REPLACE} ${SED_EXTRA} $< > $@
	${CHMOD} +x $@

%: %.in
	${SED} ${SED_REPLACE} ${SED_EXTRA} $< > $@
	${CHMOD} +x $@

P=$(PACKAGE)-$(VERSION)

all:	$(USR_BIN_FILES) functions.sh

clean:
	@rm -f $(USR_BIN_FILES) functions.sh

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS-$@) -o $@ -c $<

abuild-sudo: abuild-sudo.o
	$(LINK)

abuild-tar: abuild-tar.o
	$(LINK)

abuild-fetch: abuild-fetch.o
	$(LINK)

abuild-gzsplit: abuild-gzsplit.o
	$(LINK)

abuild-tar.static: abuild-tar.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS-$@) -o $@ -static $(LIBS-$@) $^

help:
	@echo "$(P) makefile"
	@echo "usage: make install [ DESTDIR=<path> ]"

install: $(USR_BIN_FILES) $(SAMPLES) abuild.conf functions.sh
	install -d $(DESTDIR)/$(bindir) $(DESTDIR)/$(sysconfdir) \
		$(DESTDIR)/$(datadir) $(DESTDIR)/$(mandir)/man1 \
		$(DESTDIR)/$(mandir)/man5
	for i in $(USR_BIN_FILES); do\
		install -m 755 $$i $(DESTDIR)/$(bindir)/$$i;\
	done
	chmod 4111 $(DESTDIR)/$(prefix)/bin/abuild-sudo
	for i in adduser addgroup apk; do \
		ln -fs abuild-sudo $(DESTDIR)/$(bindir)/abuild-$$i; \
	done
	for i in $(MAN_1_PAGES); do\
		install -m 644 $$i $(DESTDIR)/$(mandir)/man1/$$i;\
	done
	for i in $(MAN_5_PAGES); do\
		install -m 644 $$i $(DESTDIR)/$(mandir)/man5/$$i;\
	done
	if [ -n "$(DESTDIR)" ] || [ ! -f "/$(sysconfdir)"/abuild.conf ]; then\
		cp abuild.conf $(DESTDIR)/$(sysconfdir)/; \
	fi
	cp $(SAMPLES) $(DESTDIR)/$(prefix)/share/abuild/
	cp $(AUTOTOOLS_TOOLCHAIN_FILES) $(DESTDIR)/$(prefix)/share/abuild/
	cp functions.sh $(DESTDIR)/$(datadir)/

.gitignore: Makefile
	echo "*.tar.bz2" > $@
	for i in $(USR_BIN_FILES); do\
		echo $$i >>$@;\
	done


.PHONY: install
