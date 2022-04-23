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
