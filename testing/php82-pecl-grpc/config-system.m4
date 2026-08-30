dnl Replacement for the PECL config.m4: build only the PHP glue
dnl (src/php/ext/grpc) and link it against the system gRPC C core
dnl (libgrpc/libgpr from grpc-dev) instead of compiling the ~1460
dnl bundled translation units (gRPC core, boringssl, abseil, upb, re2...).
dnl The glue only uses the public C API (<grpc/grpc.h>, <grpc/grpc_security.h>,
dnl <grpc/credentials.h>, <grpc/support/*.h>, <grpc/slice.h>, <grpc/byte_buffer_reader.h>).
PHP_ARG_ENABLE(grpc, whether to enable grpc support,
[  --enable-grpc           Enable grpc support])

if test "$PHP_GRPC" != "no"; then
  PKG_CHECK_MODULES([GRPC], [grpc gpr])
  PHP_EVAL_INCLINE($GRPC_CFLAGS)
  PHP_EVAL_LIBLINE($GRPC_LIBS, GRPC_SHARED_LIBADD)
  PHP_ADD_INCLUDE(PHP_EXT_SRCDIR()/src/php/ext/grpc)
  PHP_ADD_LIBRARY(pthread,,GRPC_SHARED_LIBADD)
  PHP_SUBST(GRPC_SHARED_LIBADD)
  PHP_NEW_EXTENSION(grpc,
    src/php/ext/grpc/byte_buffer.c \
    src/php/ext/grpc/call.c \
    src/php/ext/grpc/call_credentials.c \
    src/php/ext/grpc/channel.c \
    src/php/ext/grpc/channel_credentials.c \
    src/php/ext/grpc/completion_queue.c \
    src/php/ext/grpc/php_grpc.c \
    src/php/ext/grpc/server.c \
    src/php/ext/grpc/server_credentials.c \
    src/php/ext/grpc/timeval.c \
    , $ext_shared, , -std=c11 -D_GNU_SOURCE -DGRPC_POSIX_FORK_ALLOW_PTHREAD_ATFORK=1)
  PHP_ADD_BUILD_DIR($ext_builddir/src/php/ext/grpc)
fi
