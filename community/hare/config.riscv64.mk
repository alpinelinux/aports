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
ARCH=riscv64

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
AARCH64_AS=aarch64-alpine-linux-musl-as
AARCH64_AR=aarch64-alpine-linux-musl-ar
AARCH64_CC=aarch64-alpine-linux-musl-cc
AARCH64_LD=aarch64-alpine-linux-musl-ld

RISCV64_AS=as
RISCV64_AR=ar
RISCV64_CC=cc
RISCV64_LD=ld

X86_64_AS=x86_64-alpine-linux-musl-as
X86_64_AR=x86_64-alpine-linux-musl-ar
X86_64_CC=x86_64-alpine-linux-musl-cc
X86_64_LD=x86_64-alpine-linux-musl-ld
