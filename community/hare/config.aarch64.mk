## Install configuration

PREFIX=/usr
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man
SRCDIR=$(PREFIX)/src

# Where to install the stdlib tree
STDLIB=$(SRCDIR)/hare/stdlib

# Default HAREPATH
LOCALSRCDIR=/usr/src/hare
HAREPATH=$(LOCALSRCDIR)/stdlib:$(LOCALSRCDIR)/third-party:$(SRCDIR)/hare/stdlib:$(SRCDIR)/hare/third-party

## Build configuration

# Platform to build for
PLATFORM=linux
ARCH=aarch64

# External tools and flags
HAREC=harec
HAREFLAGS=
QBE=qbe
AS=as
LD=ld
AR=ar
SCDOC=scdoc

# Where to store build artifacts
HARECACHE=.cache
BINOUT = .bin

# Cross-compiling settings
AARCH64_AS=as
AARCH64_AR=ar
AARCH64_CC=cc
AARCH64_LD=ld

RISCV64_AS=riscv64-alpine-linux-musl-as
RISCV64_AR=riscv64-alpine-linux-musl-ar
RISCV64_CC=riscv64-alpine-linux-musl-cc
RISCV64_LD=riscv64-alpine-linux-musl-ld

X86_64_AS=x86_64-alpine-linux-musl-as
X86_64_AR=x86_64-alpine-linux-musl-ar
X86_64_CC=x86_64-alpine-linux-musl-cc
X86_64_LD=x86_64-alpine-linux-musl-ld
