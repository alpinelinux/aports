#!/bin/sh

TARGET_ARCH="$1"
SUDO_APK=abuild-apk

# optional cross build packages
KERNEL_PKG="linux-firmware linux-vanilla"

# get abuild configurables
[ -e /usr/share/abuild/functions.sh ] || (echo "abuild not found" ; exit 1)
CBUILDROOT="$(CTARGET=$TARGET_ARCH . /usr/share/abuild/functions.sh ; echo $CBUILDROOT)"
. /usr/share/abuild/functions.sh
[ -z "$CBUILD_ARCH" ] && die "abuild is too old (use git snapshot from cross-build branch)"

# deduce aports directory
[ -z "$APORTS" ] && APORTS=$(realpath $(dirname $0)/../)
[ -e "$APORTS/main/build-base" ] || die "Unable to deduce aports base checkout"

apkbuildname() {
	echo $APORTS/main/$1/APKBUILD
}

msg() {
	[ -n "$quiet" ] && return 0
	local prompt="$GREEN>>>${NORMAL}"
	local name="${BLUE}bootstrap-${TARGET_ARCH}${NORMAL}"
        printf "${prompt} ${name}: %s\n" "$1" >&2
}

setup_sysroot() {
	[ -e "$CBUILDROOT" ] && return 0
	msg "Creating sysroot in $CBUILDROOT"
	mkdir -p "$CBUILDROOT/etc/apk/keys"
	cp -a /etc/apk/keys/* "$CBUILDROOT/etc/apk/keys"
	${SUDO_APK} add --quiet --initdb --arch $TARGET_ARCH --root $CBUILDROOT
}

create_cross_compiler() {
	msg "Building cross-compiler"

	# Prepare local build environment
	apk info --quiet --installed build-base gcc-gnat || ${SUDO_APK} add build-base gcc-gnat

	# Build and install cross binutils (--with-sysroot)
	CTARGET=$TARGET_ARCH APKBUILD=$(apkbuildname binutils) abuild up2date >& /dev/null
	if [ $? -ne 0 ]; then
		CTARGET=$TARGET_ARCH APKBUILD=$(apkbuildname binutils) abuild -r || return 1
		${SUDO_APK} add --repository "$REPODEST/main" binutils-$TARGET_ARCH || return 1
	fi

	# Build and install cross GCC
	CTARGET=$TARGET_ARCH APKBUILD=$(apkbuildname gcc) abuild up2date >& /dev/null
	if [ $? -ne 0 ]; then
		# Build bootstrap C-library for target if needed
		CHOST=$TARGET_ARCH BOOTSTRAP=nolibc APKBUILD=$(apkbuildname musl) abuild up2date >& /dev/null
		if [ $? -ne 0 ]; then
			# musl does not need GCC for headers installation, skipped step.
			# CTARGET=$TARGET_ARCH BOOTSTRAP=noheaders abuild

			# Hack: Install C-library headers for target sysroot
			CHOST=$TARGET_ARCH BOOTSTRAP=nolibc APKBUILD=$(apkbuildname musl) abuild clean unpack prepare install_sysroot_headers || return 1

			# Build minimal cross GCC (--with-newlib --enable-threads=no --disable-bootstrap)
			CTARGET=$TARGET_ARCH BOOTSTRAP=nolibc APKBUILD=$(apkbuildname gcc) abuild -r || return 1

			# Cross build bootstrap C-library for the target
			${SUDO_APK} --quiet del gcc-$TARGET_ARCH g++-$TARGET_ARCH gcc-gnat-$TARGET_ARCH
			${SUDO_APK} add --repository "$REPODEST/main" gcc-pass2-$TARGET_ARCH || return 1
			CHOST=$TARGET_ARCH BOOTSTRAP=nolibc APKBUILD=$(apkbuildname musl) abuild -r || return 1
			${SUDO_APK} --quiet del gcc-pass2-$TARGET_ARCH
		fi

		# Build cross GCC
		apk info --quiet --installed --root "$CBUILDROOT" musl-dev || \
			${SUDO_APK} --root "$CBUILDROOT" add --repository "$REPODEST/main" musl-dev \
			|| return 1
		CTARGET=$TARGET_ARCH APKBUILD=$(apkbuildname gcc) abuild -r || return 1
		${SUDO_APK} add --repository "$REPODEST/main" gcc-gnat gcc-$TARGET_ARCH g++-$TARGET_ARCH gcc-gnat-$TARGET_ARCH \
			|| return 1
	fi
}

cross_compile_base() {
	msg "Cross building base system"

	# remove possible old pass2 gcc, and add implicit host prerequisite packages
	apk info --quiet --installed gcc-pass2-$TARGET_ARCH && ${SUDO_APK} del gcc-pass2-$TARGET_ARCH
	apk info --quiet --installed gcc-gnat gcc-$TARGET_ARCH g++-$TARGET_ARCH gcc-gnat-$TARGET_ARCH || \
		${SUDO_APK} add --repository "$REPODEST/main" gcc-gnat gcc-$TARGET_ARCH g++-$TARGET_ARCH gcc-gnat-$TARGET_ARCH \
		|| return 1
	apk info --quiet --installed --root "$CBUILDROOT" libgcc musl-dev || \
		${SUDO_APK} --root "$CBUILDROOT" add --repository "$REPODEST/main" libgcc musl-dev \
		|| return 1

	# ordered cross-build
	for PKG in fortify-headers linux-headers musl libc-dev \
		   busybox busybox-initscripts binutils make pkgconf \
		   zlib openssl libfetch apk-tools \
		   gmp mpfr3 mpc1 isl cloog gcc \
		   openrc alpine-conf alpine-baselayout alpine-keys alpine-base build-base \
		   attr libcap patch sudo acl fakeroot tar \
		   pax-utils abuild openssh \
		   ncurses util-linux lvm2 popt xz cryptsetup kmod lddtree mkinitfs \
		   $KERNEL_PKG ; do

		CHOST=$TARGET_ARCH BOOTSTRAP=bootimage APKBUILD=$(apkbuildname $PKG) abuild -r || exit 1

		case "$PKG" in
		fortify-headers | libc-dev | build-base)
			# headers packages which are implicit but mandatory dependency
			apk info --quiet --installed --root "$CBUILDROOT" $PKG || \
				${SUDO_APK} --update --root "$CBUILDROOT" --repository "$REPODEST/main" add $PKG \
				|| return 1
			;;
		musl | gcc)
			# target libraries rebuilt, force upgrade
			[ "$(apk upgrade --root "$CBUILDROOT" --repository "$REPODEST/main" --available --simulate | wc -l)" -gt 1 ] &&
				${SUDO_APK} upgrade --root "$CBUILDROOT" --repository "$REPODEST/main" --available
			;;
		esac
	done
}

if [ -z "$TARGET_ARCH" ]; then
	local program=$(basename $0)
	cat <<EOF
usage: $program TARGET_ARCH

This script creates a local cross-compiler, and uses it to
cross-compile an Alpine Linux base system for new architecture.

Steps for introducing new architecture include:
- adding the compiler tripler and arch type to abuild
- adding the arch type detection to apk-tools
- adjusting build rules for packages that are arch aware:
  gcc, openssl, linux-headers
- create new kernel config for linux-vanilla

After these steps the initial cross-build can be completed
by running this with the target arch as parameter, e.g.:
	./$program aarch64

EOF
	return 1
fi

setup_sysroot && create_cross_compiler && cross_compile_base
