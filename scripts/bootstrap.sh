#!/bin/sh

set -e

TARGET_ARCH="$1"
SUDO_APK=abuild-apk

# optional cross build packages
: ${KERNEL_PKG=linux-firmware linux-lts}

# get abuild configurables
[ -e /usr/share/abuild/functions.sh ] || (echo "abuild not found" ; exit 1)
CBUILDROOT="$(CTARGET=$TARGET_ARCH . /usr/share/abuild/functions.sh ; echo $CBUILDROOT)"
. /usr/share/abuild/functions.sh
[ -z "$CBUILD_ARCH" ] && die "abuild is too old (use 2.29.0 or later)"
[ -z "$CBUILDROOT" ] && die "CBUILDROOT not set for $TARGET_ARCH"
export CBUILD

# deduce aports directory
[ -z "$APORTS" ] && APORTS=$(realpath $(dirname $0)/../)
[ -e "$APORTS/main/build-base" ] || die "Unable to deduce aports base checkout"

apkbuildname() {
	local repo="${1%%/*}"
	local pkg="${1##*/}"
	[ "$repo" = "$1" ] && repo="main"
	echo $APORTS/$repo/$pkg/APKBUILD
}

msg() {
	[ -n "$quiet" ] && return 0
	local prompt="$GREEN>>>${NORMAL}"
	local name="${BLUE}bootstrap-${TARGET_ARCH}${NORMAL}"
        printf "${prompt} ${name}: %s\n" "$1" >&2
}

if [ -z "$TARGET_ARCH" ]; then
	program=$(basename $0)
	cat <<EOF
usage: $program TARGET_ARCH

This script creates a local cross-compiler, and uses it to
cross-compile an Alpine Linux base system for new architecture.

Steps for introducing new architecture include:
- adding the compiler triplet and arch type to abuild
- adding the arch type detection to apk-tools
- adjusting build rules for packages that are arch aware:
  gcc, openssl, linux-headers
- create new kernel config for linux-lts

After these steps the initial cross-build can be completed
by running this with the target arch as parameter, e.g.:
	./$program aarch64

EOF
	return 1
fi

if [ ! -d "$CBUILDROOT" ]; then
	msg "Creating sysroot in $CBUILDROOT"
	mkdir -p "$CBUILDROOT/etc/apk/keys"
	cp -a /etc/apk/keys/* ~/.abuild/*.pub "$CBUILDROOT/etc/apk/keys"
	${SUDO_APK} add --quiet --initdb --arch $TARGET_ARCH --root $CBUILDROOT
fi

msg "Building cross-compiler"

# Build and install cross binutils (--with-sysroot)
CTARGET=$TARGET_ARCH BOOTSTRAP=nobase APKBUILD=$(apkbuildname binutils) abuild -r

if ! CHOST=$TARGET_ARCH BOOTSTRAP=nolibc APKBUILD=$(apkbuildname musl) abuild up2date 2>/dev/null; then
	# C-library headers for target
	CHOST=$TARGET_ARCH BOOTSTRAP=nocc APKBUILD=$(apkbuildname musl) abuild -r

	# Minimal cross GCC
	EXTRADEPENDS_HOST="musl-dev" \
	CTARGET=$TARGET_ARCH BOOTSTRAP=nolibc APKBUILD=$(apkbuildname gcc) abuild -r

	# Cross build bootstrap C-library for the target
	EXTRADEPENDS_BUILD="gcc-pass2-$TARGET_ARCH" \
	CHOST=$TARGET_ARCH BOOTSTRAP=nolibc APKBUILD=$(apkbuildname musl) abuild -r
fi

# Full cross GCC
EXTRADEPENDS_TARGET="musl musl-dev" \
CTARGET=$TARGET_ARCH BOOTSTRAP=nobase APKBUILD=$(apkbuildname gcc) abuild -r

# Cross build-base
CTARGET=$TARGET_ARCH BOOTSTRAP=nobase APKBUILD=$(apkbuildname build-base) abuild -r

msg "Cross building base system"

# Implicit dependencies for early targets
EXTRADEPENDS_TARGET="libgcc libstdc++ musl-dev"

# On a few architectures like riscv64 we need to account for
# gcc requiring -ltomic to be set explicitly if a C[++]11 program
# uses atomics (e.g. #include <atomic>):
# https://github.com/riscv/riscv-gnu-toolchain/issues/183#issuecomment-253721765
# The reason gcc itself is needed is because .so is in that package,
# not in libatomic.
if [ "$TARGET_ARCH" = "riscv64" ]; then
	NEEDS_LIBATOMIC="yes"
fi

# ordered cross-build
for PKG in fortify-headers linux-headers musl libc-dev pkgconf zlib \
	   openssl ca-certificates libmd \
	   gmp mpfr4 mpc1 isl22 cloog libucontext binutils gcc \
	   libbsd libtls-standalone busybox busybox-initscripts make \
	   apk-tools file \
	   openrc alpine-conf alpine-baselayout alpine-keys alpine-base patch build-base \
	   attr libcap acl fakeroot tar \
	   lzip abuild ncurses libedit openssh \
	   libcap-ng util-linux libaio lvm2 popt xz \
	   json-c argon2 cryptsetup zstd kmod lddtree mkinitfs \
	   community/go libffi community/ghc \
	   brotli libev c-ares cunit nghttp2 curl \
	   pcre libssh2 community/http-parser community/libgit2 \
	   libxml2 pax-utils llvm11 community/rust \
	   $KERNEL_PKG ; do

	if [ "$NEEDS_LIBATOMIC" = "yes" ]; then
		EXTRADEPENDS_BUILD="libatomic gcc-$TARGET_ARCH g++-$TARGET_ARCH"
	fi
	EXTRADEPENDS_TARGET="$EXTRADEPENDS_TARGET"  EXTRADEPENDS_BUILD="$EXTRADEPENDS_BUILD" \
	CHOST=$TARGET_ARCH BOOTSTRAP=bootimage APKBUILD=$(apkbuildname $PKG) abuild -r

	case "$PKG" in
	fortify-headers | libc-dev)
		# Additional implicit dependencies once built
		EXTRADEPENDS_TARGET="$EXTRADEPENDS_TARGET $PKG"
		;;
	gcc)
		if [ "$NEEDS_LIBATOMIC" = "yes" ]; then
			EXTRADEPENDS_TARGET="libatomic gcc $EXTRADEPENDS_TARGET"
		fi
		;;
	build-base)
		# After build-base, that alone is sufficient dependency in the target
		EXTRADEPENDS_TARGET="busybox $PKG"
		;;
	esac
done
