progs-y			+= apk
apk-objs		:= state.o database.o package.o archive.o \
			   version.o io.o url.o gunzip.o blob.o \
			   hash.o md5.o apk.o \
			   add.o del.o update.o info.o search.o \
			   ver.o index.o fetch.o audit.o
CFLAGS_apk.o		:= -DAPK_VERSION=\"$(FULL_VERSION)\"

progs-$(STATIC)		+= apk.static
apk.static-objs		:= $(apk-objs)
LDFLAGS_apk.static	:= -static
LDFLAGS_apk		+= -nopie

LIBS			:= /usr/lib/libz.a

install::
	$(INSTALLDIR) $(DESTDIR)$(SBINDIR)
	$(INSTALL) $(obj)/apk $(DESTDIR)$(SBINDIR)
