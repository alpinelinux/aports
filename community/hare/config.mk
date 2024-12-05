# install locations
PREFIX = /usr/
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man
SRCDIR = $(PREFIX)/src
STDLIB = $(SRCDIR)/hare/stdlib

# variables used during build
PLATFORM = linux
ARCH = @CARCH@
HAREFLAGS =
HARECFLAGS =
QBEFLAGS =
ASFLAGS =
LDLINKFLAGS = --gc-sections -z noexecstack

# commands used by the build script
HAREC = harec
QBE = qbe
AS = as
LD = ld
SCDOC = scdoc

# build locations
HARECACHE = .cache
BINOUT = .bin

# variables that will be embedded in the binary with -D definitions
HAREPATH = $(SRCDIR)/hare/stdlib:$(SRCDIR)/hare/third-party
VERSION = @VERSION@

# For cross-compilation, modify the variables below
AARCH64_AS=aarch64-alpine-linux-musl-as
AARCH64_CC=aarch64-alpine-linux-musl-cc
AARCH64_LD=aarch64-alpine-linux-musl-ld

RISCV64_AS=riscv64-alpine-linux-musl-as
RISCV64_CC=riscv64-alpine-linux-musl-cc
RISCV64_LD=riscv64-alpine-linux-musl-ld

x86_64_AS=x86_64-alpine-linux-musl-as
x86_64_CC=x86_64-alpine-linux-musl-cc
x86_64_LD=x86_64-alpine-linux-musl-ld
