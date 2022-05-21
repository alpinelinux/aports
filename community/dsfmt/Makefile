VERSION       = $(file < VERSION)
VERSION_MAJOR = $(word 1, $(subst ., ,$(VERSION)))
VERSION_MINOR = $(word 2, $(subst ., ,$(VERSION)))

prefix        = /usr
includedir    = $(prefix)/include
libdir        = $(prefix)/lib
DESTDIR       = /

SFMT_MEXP     = 19937

# Based on the upstream Makefile and https://github.com/JuliaLang/julia/blob/master/deps/dsfmt.mk.
DEFINES       = -DDSFMT_MEXP=$(SFMT_MEXP) -DDSFMT_DO_NOT_USE_OLD_NAMES -DDSFMT_SHLIB
OPTFLAGS      = -O3 -DNDEBUG -finline-functions -fomit-frame-pointer -fno-strict-aliasing \
--param max-inline-insns-single=1800 \
--param inline-unit-growth=500 \
--param large-function-growth=900

ifeq ($(ARCH), x86_64)
OPTFLAGS     += -msse2 -DHAVE_SSE2
endif

CFLAGS       += -fPIC -std=c99 -Wmissing-prototypes -Wall $(OPTFLAGS) $(DEFINES)
LDFLAGS      += -Wl,--no-undefined -Wl,-z,noexecstack -shared

TARGET        = dSFMT

HEADERS       = $(TARGET).h $(TARGET)-params.h $(TARGET)-common.h
SOURCE        = $(TARGET).c $(HEADERS)
OBJECT        = $(TARGET).o

STATIC_LIB    = lib$(TARGET).a
SHARED_LIB0   = lib$(TARGET).so
SHARED_LIB    = $(SHARED_LIB0).$(VERSION)
SONAME        = $(SHARED_LIB0).$(VERSION_MAJOR).$(VERSION_MINOR)

.PHONY: build check install

build: $(SHARED_LIB) $(STATIC_LIB) $(TARGET).pc

check: test
	LD_LIBRARY_PATH=. ./test > test-M$(SFMT_MEXP).txt
	diff -w test-M$(SFMT_MEXP).txt dSFMT.$(SFMT_MEXP).out.txt || exit 1

install: $(SHARED_LIB) $(STATIC_LIB) $(TARGET).pc
	@echo $(DESTDIR)
	mkdir -p $(DESTDIR)$(libdir)
	cp -a $(SHARED_LIB) $(SONAME) $(SHARED_LIB0) $(DESTDIR)$(libdir)/
	install -D -m644 $(HEADERS) -t $(DESTDIR)$(includedir)
	install -D -m644 $(TARGET).pc -t $(DESTDIR)$(libdir)/pkgconfig/

clean:
	$(RM) *.o *.so *.so.* *~ test-*-M*

$(SHARED_LIB): $(OBJECT)
	$(LINK.o) $(LDFLAGS) -Wl,-soname,$(SONAME) -o $(SHARED_LIB) $<
	ln -s $(SHARED_LIB) $(SONAME)
	ln -s $(SHARED_LIB) $(SHARED_LIB0)

$(STATIC_LIB): $(OBJECT)
	$(AR) rcs $@ $^

$(TARGET).pc:
	@sed -e 's|@VERSION@|$(VERSION)|g' $(TARGET).pc.in > $(TARGET).pc

test: test.c $(HEADERS) $(SHARED_LIB)
	$(CC) $(CFLAGS) -L. -o $@ test.c -l$(TARGET)

.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) -c $<

