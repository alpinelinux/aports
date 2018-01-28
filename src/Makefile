PKG_CONFIG ?= pkg-config
LUAAPK ?= yes

OPENSSL_CFLAGS		:= $(shell $(PKG_CONFIG) --cflags openssl)
OPENSSL_LIBS		:= $(shell $(PKG_CONFIG) --libs openssl)

ZLIB_CFLAGS		:= $(shell $(PKG_CONFIG) --cflags zlib)
ZLIB_LIBS		:= $(shell $(PKG_CONFIG) --libs zlib)

# lua module
ifneq ($(LUAAPK),)
LUA_VERSION		?= 5.2
LUA_PC			?= lua$(LUA_VERSION)
LUA_LIBDIR		?= /usr/lib/lua/$(LUA_VERSION)
LIBAPK			:= YesPlease
shlibs-y		+= apk.so
apk.so-objs		:= lua-apk.o
CFLAGS_lua-apk.o	:= -DAPK_VERSION=\"$(FULL_VERSION)\" \
			   $(shell $(PKG_CONFIG) $(LUA_PC) --cflags)
LUA_LIB-y		:= $(obj)/apk.so
install-LUA_LIB-y	:= $(INSTALLDIR) $(DESTDIR)$(LUA_LIBDIR) && \
			   $(INSTALL) $(LUA_LIB-y) $(DESTDIR)$(LUA_LIBDIR)
endif

progs-y			+= apk
apk-objs		:= apk.o add.o del.o fix.o update.o info.o list.o \
			   search.o upgrade.o cache.o ver.o index.o fetch.o \
			   audit.o verify.o dot.o policy.o stats.o manifest.o

libapk.so-objs		:= common.o database.o package.o archive.o \
			   version.o io.o url.o gunzip.o blob.o hash.o print.o \
			   commit.o solver.o
libapk.so-libs		:= libfetch/libfetch.a

ifeq ($(TEST),y)
progs-y			+= apk-test
apk-test-objs		:= apk-test.o $(filter-out apk.o, $(apk-objs))
endif

ifeq ($(SHARED_LIBAPK),)
apk-objs		+= $(libapk.so-objs)
apk-test-objs		+= $(libapk.so-objs)
apk.so-objs		+= $(libapk.so-objs)
apk-libs		+= $(libapk.so-libs)
apk-test-libs		+= $(libapk.so-libs)
apk.so-libs		+= $(libapk.so-libs)
else
LIBAPK			:= YesPlease
LIBS_apk		:= -lapk
LIBS_apk-test		:= -lapk
LIBS_apk.so		:= -L$(obj) -lapk
endif

ifneq ($(SHARED_LIBAPK),)
shlibs-y		+= libapk.so
LIBAPK-y		:= $(obj)/libapk.so
install-LIBAPK-y	:= $(INSTALLDIR) $(DESTDIR)$(LIBDIR) && \
			   $(INSTALL) $(LIBAPK-y) $(DESTDIR)$(LIBDIR)
endif

CFLAGS_ALL		+= -D_ATFILE_SOURCE -Ilibfetch
CFLAGS_apk.o		:= -DAPK_VERSION=\"$(FULL_VERSION)\"
CFLAGS_apk-static.o	:= -DAPK_VERSION=\"$(FULL_VERSION)\" -DOPENSSL_NO_ENGINE
CFLAGS_apk-test.o	:= -DAPK_VERSION=\"$(FULL_VERSION)\" -DOPENSSL_NO_ENGINE -DTEST_MODE

progs-$(STATIC)		+= apk.static
apk.static-objs		:= $(filter-out apk.o,$(apk-objs)) apk-static.o
apk.static-libs		:= $(apk-libs)
LDFLAGS_apk.static	:= -static
LIBS_apk.static		:= -Wl,--as-needed -ldl -Wl,--no-as-needed
LDFLAGS_apk		+= -L$(obj)
LDFLAGS_apk-test	+= -L$(obj)

CFLAGS_ALL		+= $(OPENSSL_CFLAGS) $(ZLIB_CFLAGS)
LIBS			:= -Wl,--as-needed \
				$(OPENSSL_LIBS) $(ZLIB_LIBS) \
			   -Wl,--no-as-needed

$(obj)/apk: $(LIBAPK-y)

$(obj)/apk.so: $(LIBAPK-y)

install: $(obj)/apk $(LIBAPK-y) $(LUA_LIB-y)
	$(INSTALLDIR) $(DESTDIR)$(SBINDIR)
	$(INSTALL) $(obj)/apk $(DESTDIR)$(SBINDIR)
	$(install-LIBAPK-y)
	$(install-LUA_LIB-y)

