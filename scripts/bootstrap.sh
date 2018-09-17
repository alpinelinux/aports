#!/bin/sh

set -e

TARGET_ARCH="$1"
SUDO_APK=abuild-apk

# optional cross build packages
KERNEL_PKG="linux-firmware linux-vanilla"

# get abuild configurables
[ -e /usr/share/abuild/functions.sh ] || (echo "abuild not found" ; exit 1)
CBUILDROOT="$(CTARGET=$TARGET_ARCH . /usr/share/abuild/functions.sh ; echo $CBUILDROOT)"
. /usr/share/abuild/functions.sh
[ -z "$CBUILD_ARCH" ] && die "abuild is too old (use 2.29.0 or later)"
[ -z "$CBUILDROOT" ] && die "CBUILDROOT not set for $TARGET_ARCH"

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
  gcc, libressl, linux-headers
- create new kernel config for linux-vanilla

After these steps the initial cross-build can be completed
by running this with the target arch as parameter, e.g.:
	./$program aarch64

EOF
	return 1
fi

if [ ! -d "$CBUILDROOT" ]; then
	msg "Creating sysroot in $CBUILDROOT"
	mkdir -p "$CBUILDROOT/etc/apk/keys"
	cp -a /etc/apk/keys/* "$CBUILDROOT/etc/apk/keys"
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

# add implicit target prerequisite packages
apk info --quiet --installed --root "$CBUILDROOT" libgcc libstdc++ musl-dev || \
	${SUDO_APK} --root "$CBUILDROOT" add --repository "$REPODEST/main" libgcc libstdc++ musl-dev

# ordered cross-build
for PKG in fortify-headers linux-headers musl libc-dev pkgconf zlib \
	   libressl busybox busybox-initscripts binutils make \
	   apk-tools file \
	   gmp mpfr3 mpc1 isl cloog gcc \
	   openrc alpine-conf alpine-baselayout alpine-keys alpine-base build-base \
	   attr libcap patch sudo acl fakeroot tar \
	   pax-utils lzip abuild openssh \
	   ncurses libcap-ng util-linux libaio lvm2 popt xz \
	   json-c argon2 cryptsetup kmod lddtree mkinitfs \
	   community/go libffi community/ghc \
	   $KERNEL_PKG ; do

	CHOST=$TARGET_ARCH BOOTSTRAP=bootimage APKBUILD=$(apkbuildname $PKG) abuild -r

	case "$PKG" in
	fortify-headers | libc-dev | build-base)
		# headers packages which are implicit but mandatory dependency
		apk info --quiet --installed --root "$CBUILDROOT" $PKG || \
			${SUDO_APK} --update --root "$CBUILDROOT" --repository "$REPODEST/main" add $PKG
		;;
	musl | gcc)
		# target libraries rebuilt, force upgrade
		[ "$(apk upgrade --root "$CBUILDROOT" --repository "$REPODEST/main" --available --simulate | wc -l)" -gt 1 ] &&
			${SUDO_APK} upgrade --root "$CBUILDROOT" --repository "$REPODEST/main" --available
		;;
	esac
done
