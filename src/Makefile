PKGDEPS := openssl zlib

ifeq ($(shell pkg-config --print-errors --exists $(PKGDEPS) || echo fail),fail)
$(error Build dependencies are not met)
endif

shlibs-y		+= libapk.so
libapk.so-objs		:= common.o state.o database.o package.o archive.o \
			   version.o io.o url.o gunzip.o blob.o hash.o print.o

progs-y			+= apk
apk-objs		:= apk.o add.o del.o fix.o update.o info.o \
			   search.o upgrade.o cache.o ver.o index.o fetch.o \
			   audit.o verify.o

CFLAGS			+= -D_ATFILE_SOURCE
CFLAGS_apk.o		:= -DAPK_VERSION=\"$(FULL_VERSION)\"
LIBS_apk		:= -lapk

progs-$(STATIC)		+= apk.static
apk.static-objs		:= $(apk-objs)
LDFLAGS_apk.static	:= -static
LDFLAGS_apk		+= -nopie -L$(obj)

CFLAGS			+= $(shell pkg-config --cflags $(PKGDEPS))
LIBS			:= -Wl,--as-needed \
				$(shell pkg-config --libs $(PKGDEPS)) \
			   -Wl,--no-as-needed


$(obj)/apk:	$(obj)/libapk.so

install: $(obj)/apk $(obj)/libapk.so
	$(INSTALLDIR) $(DESTDIR)$(SBINDIR) $(DESTDIR)$(LIBDIR)
	$(INSTALL) $(obj)/apk $(DESTDIR)$(SBINDIR)
	$(INSTALL) $(obj)/libapk.so $(DESTDIR)$(LIBDIR)

