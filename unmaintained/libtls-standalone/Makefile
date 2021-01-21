PREFIX = /usr/local
EXEC_PREFIX = ${PREFIX}
LIBDIR = ${PREFIX}/lib
INCLUDEDIR = ${PREFIX}/include

LIBRESSL_VERSION = 1.0
LIBTLS_SOVERSION = 1
LIBTLS_FULLVERSION = 1.0.0

OPENSSL_CFLAGS = $(shell pkgconf openssl --cflags)
OPENSSL_LIBS = $(shell pkgconf openssl --libs)

CFLAGS = -Wall -Wno-pointer-sign -fPIC -DPIC -shared -include tls_compat.h -isystem ${LIBRESSL_SRCDIR}/include/compat ${OPENSSL_CFLAGS} -I ${LIBRESSL_SRCDIR}/include \
	-D__BEGIN_HIDDEN_DECLS= -D__END_HIDDEN_DECLS=
LDFLAGS = -fPIC -DPIC -shared -Wl,-soname,libtls-standalone.so.${LIBTLS_SOVERSION} ${OPENSSL_LIBS}

SOURCES = \
	tls_compat.c				\
	${LIBRESSL_SRCDIR}/tls/tls.c		\
	${LIBRESSL_SRCDIR}/tls/tls_bio_cb.c	\
	${LIBRESSL_SRCDIR}/tls/tls_client.c	\
	${LIBRESSL_SRCDIR}/tls/tls_config.c	\
	${LIBRESSL_SRCDIR}/tls/tls_conninfo.c	\
	${LIBRESSL_SRCDIR}/tls/tls_keypair.c	\
	${LIBRESSL_SRCDIR}/tls/tls_ocsp.c	\
	${LIBRESSL_SRCDIR}/tls/tls_peer.c	\
	${LIBRESSL_SRCDIR}/tls/tls_server.c	\
	${LIBRESSL_SRCDIR}/tls/tls_util.c	\
	${LIBRESSL_SRCDIR}/tls/tls_verify.c	\
	${LIBRESSL_SRCDIR}/crypto/compat/arc4random.c		\
	${LIBRESSL_SRCDIR}/crypto/compat/arc4random_uniform.c	\
	${LIBRESSL_SRCDIR}/crypto/compat/freezero.c		\
	${LIBRESSL_SRCDIR}/crypto/compat/reallocarray.c		\
	${LIBRESSL_SRCDIR}/crypto/compat/timingsafe_memcmp.c

OBJECTS = ${SOURCES:.c=.o}

all: dumpconfig libtls-standalone.so.${LIBTLS_FULLVERSION} libtls-standalone.so.${LIBTLS_SOVERSION} libtls-standalone.pc

dumpconfig:
	@echo "Building with:"
	@echo "  LIBRESSL_SRCDIR = ${LIBRESSL_SRCDIR}"
	@echo "  OPENSSL_CFLAGS  = ${OPENSSL_CFLAGS}"
	@echo "  OPENSSL_LIBS    = ${OPENSSL_LIBS}"

.c.o:
	${CC} ${CFLAGS} -o $@ -c $<

libtls-standalone.so.${LIBTLS_SOVERSION}: libtls-standalone.so.${LIBTLS_FULLVERSION}
	ln -s $< $@

libtls-standalone.so.${LIBTLS_FULLVERSION}: ${OBJECTS}
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ ${OBJECTS} ${OPENSSL_LIBS}

check: test_program
	LD_LIBRARY_PATH=$$PWD ./test_program

test_program: test_program.c
	${CC} -I${LIBRESSL_SRCDIR}/include -o $@ test_program.c ./libtls-standalone.so.${LIBTLS_FULLVERSION}

clean:
	rm -f ${OBJECTS}
	rm -f libtls-standalone.so.${LIBTLS_FULLVERSION} libtls-standalone.so.${LIBTLS_SOVERSION}
	rm -f test_program

install:
	install -D -m755 libtls-standalone.so.${LIBTLS_FULLVERSION} ${DESTDIR}${PREFIX}/lib/libtls-standalone.so.${LIBTLS_FULLVERSION}
	ln -sf libtls-standalone.so.${LIBTLS_FULLVERSION} ${DESTDIR}${PREFIX}/lib/libtls-standalone.so.${LIBTLS_SOVERSION}
	ln -sf libtls-standalone.so.${LIBTLS_FULLVERSION} ${DESTDIR}${PREFIX}/lib/libtls-standalone.so

	install -D -m644 ${LIBRESSL_SRCDIR}/include/tls.h ${DESTDIR}${PREFIX}/include/libtls-standalone/tls.h

	install -D -m644 libtls-standalone.pc ${DESTDIR}${PREFIX}/lib/pkgconfig/libtls-standalone.pc

libtls-standalone.pc: ${LIBRESSL_SRCDIR}/libtls.pc.in
	sed -e s:@prefix@:${PREFIX}:g \
	    -e s:@exec_prefix@:${EXEC_PREFIX}:g \
	    -e s:@libdir@:${LIBDIR}:g \
	    -e s:@includedir@:${INCLUDEDIR}/libtls-standalone:g \
	    -e s:@LIBS@:-ltls-standalone:g \
	    -e s:@PLATFORM_LDADD@::g \
	    -e s:@VERSION@:${LIBRESSL_VERSION}:g \
	    -e /^Libs:/s:-ltls:-ltls-standalone:g \
	    $< > libtls-standalone.pc

.DUMMY: check dumpconfig clean install
