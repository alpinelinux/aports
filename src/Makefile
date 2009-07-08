PKGDEPS := openssl

ifeq ($(shell pkg-config --print-errors --exists $(PKGDEPS) || echo fail),fail)
$(error Build dependencies are not met)
endif

progs-y			+= apk
apk-objs		:= state.o database.o package.o archive.o \
			   version.o io.o url.o gunzip.o blob.o hash.o apk.o \
			   add.o del.o update.o info.o search.o upgrade.o \
			   cache.o ver.o index.o fetch.o audit.o
CFLAGS_apk.o		:= -DAPK_VERSION=\"$(FULL_VERSION)\"

progs-$(STATIC)		+= apk.static
apk.static-objs		:= $(apk-objs)
LDFLAGS_apk.static	:= -static
LDFLAGS_apk		+= -nopie

LIBS			+= $(shell pkg-config --libs $(PKGDEPS))
LIBS			+= /usr/lib/libz.a

install::
	$(INSTALLDIR) $(DESTDIR)$(SBINDIR)
	$(INSTALL) $(obj)/apk $(DESTDIR)$(SBINDIR)
