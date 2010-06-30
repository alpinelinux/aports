PKGDEPS := openssl zlib

# lua module
ifneq ($(LUAAPK),)
LIBAPK			:= YesPlease
PKGDEPS			+= lua
shlibs-y		+= apk.so
apk.so-objs		:= lua-apk.o
CFLAGS_lua-apk.o	:= -DAPK_VERSION=\"$(FULL_VERSION)\"
LIBS_apk.so		:= -L$(obj) -lapk
LUA_LIB-y		:= $(obj)/apk.so
install-LUA_LIB-y	:= $(INSTALLDIR) $(DESTDIR)$(LUA_LIBDIR) \
			   $(INSTALL) $(LUA_LIB-y) $(DESTDIR)$(LUA_LIBDIR)
endif

ifeq ($(shell pkg-config --print-errors --exists $(PKGDEPS) || echo fail),fail)
$(error Build dependencies are not met)
endif

progs-y			+= apk
apk-objs		:= apk.o add.o del.o fix.o update.o info.o \
			   search.o upgrade.o cache.o ver.o index.o fetch.o \
			   audit.o verify.o

libapk.so-objs		:= common.o state.o database.o package.o archive.o \
			   version.o io.o url.o gunzip.o blob.o hash.o print.o

ifeq ($(SHARED_LIBAPK),)
apk-objs		+= $(libapk.so-objs)
else
LIBAPK			:= YesPlease
LIBS_apk		:= -lapk
endif

ifneq ($(LIBAPK),)
shlibs-y		+= libapk.so
LIBAPK-y		:= $(obj)/libapk.so
install-LIBAPK-y	:= $(INSTALLDIR) $(DESTDIR)$(LIBDIR) \
			   $(INSTALL) $(LIBAPK-y) $(DESTDIR)$(LIBDIR)
endif

CFLAGS_ALL		+= -D_ATFILE_SOURCE
CFLAGS_apk.o		:= -DAPK_VERSION=\"$(FULL_VERSION)\"
CFLAGS_apk-static.o	:= -DAPK_VERSION=\"$(FULL_VERSION)\" -DOPENSSL_NO_ENGINE

progs-$(STATIC)		+= apk.static
apk.static-objs		:= $(filter-out apk.o,$(apk-objs)) apk-static.o
LDFLAGS_apk.static	:= -static
LDFLAGS_apk		+= -nopie -L$(obj)

CFLAGS_ALL		+= $(shell pkg-config --cflags $(PKGDEPS))
LIBS			:= -Wl,--as-needed \
				$(shell pkg-config --libs $(PKGDEPS)) \
			   -Wl,--no-as-needed

$(obj)/apk:	$(LIBAPK-y)

$(obj)/apk.so:	$(obj)/libapk.so

install: $(obj)/apk $(LIBAPK-y) $(LUA_LIB-y)
	$(INSTALLDIR) $(DESTDIR)$(SBINDIR)
	$(INSTALL) $(obj)/apk $(DESTDIR)$(SBINDIR)
	$(install-LIBAPK-y)
	$(install-LUA_LIB-y)

