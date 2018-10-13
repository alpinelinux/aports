PREFIX = /usr/local
EXEC_PREFIX = ${PREFIX}
LIBDIR = ${PREFIX}/lib
INCLUDEDIR = ${PREFIX}/include

LIBRESSL_VERSION = 1.0
LIBTLS_SOVERSION = 17
LIBTLS_FULLVERSION = 17.0.1

OPENSSL_CFLAGS = $(shell pkgconf openssl --cflags)
OPENSSL_LIBS = $(shell pkgconf openssl --libs)

CFLAGS = -Wall -Wno-pointer-sign -fPIC -DPIC -shared -include tls_compat.h -isystem ${LIBRESSL_SRCDIR}/include/compat ${OPENSSL_CFLAGS} -I ${LIBRESSL_SRCDIR}/include \
	-D__BEGIN_HIDDEN_DECLS= -D__END_HIDDEN_DECLS=
LDFLAGS = -fPIC -DPIC -shared -Wl,-soname,libtls.so.${LIBTLS_SOVERSION} ${OPENSSL_LIBS}

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

all: dumpconfig libtls.so.${LIBTLS_FULLVERSION} libtls.pc

dumpconfig:
	@echo "Building with:"
	@echo "  LIBRESSL_SRCDIR = ${LIBRESSL_SRCDIR}"
	@echo "  OPENSSL_CFLAGS  = ${OPENSSL_CFLAGS}"
	@echo "  OPENSSL_LIBS    = ${OPENSSL_LIBS}"

.c.o:
	${CC} ${CFLAGS} -o $@ -c $<

libtls.so.${LIBTLS_FULLVERSION}: ${OBJECTS}
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ ${OBJECTS} ${OPENSSL_LIBS}

check: test_program
	LD_PRELOAD=./libtls.so.${LIBTLS_FULLVERSION} ./test_program

test_program: test_program.c
	${CC} -I${LIBRESSL_SRCDIR}/include -o $@ test_program.c ./libtls.so.${LIBTLS_FULLVERSION}

clean:
	rm -f ${OBJECTS}
	rm -f libtls.so.${LIBTLS_FULLVERSION}
	rm -f test_program

install:
	install -D -m755 libtls.so.${LIBTLS_FULLVERSION} ${DESTDIR}${PREFIX}/lib/libtls.so.${LIBTLS_FULLVERSION}
	ln -sf libtls.so.${LIBTLS_FULLVERSION} ${DESTDIR}${PREFIX}/lib/libtls.so.${LIBTLS_SOVERSION}

	install -D -m644 ${LIBRESSL_SRCDIR}/include/tls.h ${DESTDIR}${PREFIX}/include/tls.h

	install -D -m644 libtls.pc ${DESTDIR}${PREFIX}/lib/pkgconfig/libtls.pc

libtls.pc: ${LIBRESSL_SRCDIR}/libtls.pc.in
	sed -e s:@prefix@:${PREFIX}:g \
	    -e s:@exec_prefix@:${EXEC_PREFIX}:g \
	    -e s:@libdir@:${LIBDIR}:g \
	    -e s:@includedir@:${INCLUDEDIR}:g \
	    -e s:@LIBS@:-ltls:g \
	    -e s:@PLATFORM_LDADD@::g \
	    -e s:@VERSION@:${LIBRESSL_VERSION}:g \
	    ${LIBRESSL_SRCDIR}/libtls.pc.in > libtls.pc

.DUMMY: check dumpconfig clean install
