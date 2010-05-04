
PACKAGE		:= abuild
VERSION		:= 2.2

prefix		?= /usr
sysconfdir	?= /etc
datadir		?= $(prefix)/share/$(PACKAGE)
apkcache	?= ~/.cache/apks

SCRIPTS		:= abuild devbuild mkalpine buildrepo abuild-keygen \
		abuild-sign newapkbuild
USR_BIN_FILES	:= $(SCRIPTS) abuild-tar
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

SED_REPLACE	:= -e 's:@VERSION@:$(FULL_VERSION):g' \
			-e 's:@prefix@:$(prefix):g' \
			-e 's:@sysconfdir@:$(sysconfdir):g' \
			-e 's:@datadir@:$(datadir):g' \
			-e 's:@apkcache@:$(apkcache):g'

SSL_LIBS	:= $(shell pkg-config --libs openssl)

.SUFFIXES:	.sh.in .in
.sh.in.sh:
	${SED} ${SED_REPLACE} ${SED_EXTRA} $< > $@
	${CHMOD} +x $@

.in:
	${SED} ${SED_REPLACE} ${SED_EXTRA} $< > $@
	${CHMOD} +x $@

P=$(PACKAGE)-$(VERSION)

all: 	$(USR_BIN_FILES)

clean:
	@rm -f $(USR_BIN_FILES)

abuild-tar:	abuild-tar.c
	$(CC) -o $@ $(SSL_LIBS) $^

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

dist:	$(P).tar.bz2

$(P).tar.bz2:	$(DISTFILES)
	rm -rf $(P)
	mkdir -p $(P)
	cp $(DISTFILES) $(P)/
	tar -cjf $@ $(P)
	rm -rf $(P)

.PHONY: install dist
