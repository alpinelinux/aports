#!/bin/sh -e

# update-kernel
#
# Kernel and firmware update script for Alpine installations set up
# with setup-bootable
#
# Copyright (c) 2014 Timo Teräs
# Copyright (c) 2014-2015 Kaarle Ritvanen


SCRIPT=update-kernel
VIRTUAL=.tmp-$SCRIPT

SUPERUSER=
[ $(id -u) -eq 0 ] && SUPERUSER=Y
if [ -z "$SUPERUSER" ] && [ -z "$FAKEROOTKEY" ]; then
	exec fakeroot "$0" "$@"
fi

ARCH=
BUILDDIR=
FLAVOR=
MEDIA=
MNTDIR=
PACKAGES=
MKINITFS_ARGS=
REPOSITORIES_FILE=/etc/apk/repositories
SIGNALS="HUP INT TERM"
TMPDIR=
features=

error() {
	echo "$SCRIPT: $1" >&2
}

usage() {
	[ "$2" ] && error "$2"
	local opts="[-F <feature>]... [-p <package>]..."
	local dest_args="[-a <arch>] <dest_dir>"
	cat >&2 <<-__EOF__

		Syntax: $SCRIPT $opts [$dest_args]
		        $SCRIPT -f <flavor> $opts $dest_args
		        $SCRIPT -b <build_dir> $opts [$dest_args]

		Options: -a|--arch <arch>        Install kernel for specified architecture
		         -b|--build <build_dir>  Install custom-built kernel
		         -f|--flavor <flavor>    Install kernel of specified flavor
		         -F|--feature <feature>  Enable initfs feature
		         -p|--package <package>  Additional module or firmware package
		         -v|--verbose            Verbose output
			 -k|--apk-pubkey <key>   Include given key in initramfs
		         -K|--hostkeys           Include host keys in initramfs
		         -M|--media              Boot media directory layout
		         --repositories-file <f> apk repositories file

	__EOF__
	exit $1
}

QUIET_OPT="--quiet"
OPTS=$(getopt -l arch:,build-dir:,flavor:,feature:,help,package:,verbose,apk-pubkey:,hostkeys,media,repositories-file: \
	-n $SCRIPT -o a:b:f:F:hp:vk:KM -- "$@") || usage 1

eval set -- "$OPTS"
while :; do
	case "$1" in
	-a|--arch)
		shift
		ARCH=$1
		;;
	-b|--build-dir)
		shift
		BUILDDIR=$1
		;;
	-f|--flavor)
		shift
		FLAVOR=$1
		;;
	-F|--feature)
		shift
		features="$features $1"
		;;
	-h|--help)
		echo "$SCRIPT 3.5.0-r3" >&2
		usage 0
		;;
	-p|--package)
		shift
		PACKAGES="$PACKAGES $1"
		;;
	-v|--verbose)
		QUIET_OPT=
		;;
	-k|--apk-pubkey)
		shift
		APK_PUBKEY="$1"
		;;
	-K|--hostkeys)
		MKINITFS_ARGS="$MKINITFS_ARGS -K"
		;;
	-M|--media)
		MEDIA=yes
		;;
	--repositories-file)
		shift
		REPOSITORIES_FILE=$1
		;;
	--)
		break
		;;
	esac
	shift
done

DESTDIR=$2


[ "$BUILDDIR" -a "$FLAVOR" ] && \
	usage 1 "Cannot specify both build directory and flavor"

if [ -z "$DESTDIR" ]; then
	[ "$ARCH" ] && \
		usage 1 "Cannot specify architecture when updating the current kernel"

	[ "$FLAVOR" ] && \
		usage 1 "Cannot specify flavor when updating the current kernel"

	[ "$SUPERUSER" ] || \
		usage 1 "Specify destination directory or run as superuser"

	while read MOUNT; do
		set -- $MOUNT
		[ $2 = /.modloop ] || continue
		DESTDIR=$(dirname $(busybox losetup $1 | cut -d " " -f 3))
		MNTDIR=$(dirname "$DESTDIR")
		break
	done < /proc/mounts

	if [ -z "$MNTDIR" ]; then
		error "Module loopback device not mounted"
		exit 1
	fi
fi

remount() {
	mount $1 -o remount "$MNTDIR"
}


ignore_sigs() {
	trap "" $SIGNALS
}

clean_up() {
	set +e
	ignore_sigs

	if [ "$SUPERUSER" ] && [ -z "$FAKEROOTKEY" ]; then
		apk del $QUIET_OPT $VIRTUAL
	fi
	rm -fr $TMPDIR
}

trap clean_up EXIT $SIGNALS


if [ "$SUPERUSER" ] && [ -z "$FAKEROOTKEY" ]; then
	apk add $QUIET_OPT --update-cache -t $VIRTUAL mkinitfs squashfs-tools kmod
fi

if [ -z "$features" ]; then
	. /etc/mkinitfs/mkinitfs.conf
fi

if [ -z "$FLAVOR" ]; then
	FLAVOR=$(uname -r | cut -d - -f 3-)
	[ "$FLAVOR" ] || FLAVOR=vanilla
fi

STAGE KERNEL/MODULES/FIRMWARE/DTBS/HEADERS
	if [ "$BUILDDIR" ]; then
		kerneltool stage "$BUILDDIR" ... firmware ...
	else
		kerneltool stage latest${_kflavor:+-$_kflavor} modules
	fi
BUILD INITFS MODULES
	kerneltool mkmodcpio <modglobs>

BUILD MODLOOP
	kerneltool mkmodloop <modglobs>

(mkinitfs)
STAGE APKROOT
	apkroottool init <dir> <arch>
	apkroottool setup <dir>
	apkroottool <dir> add --no-scripts $packages
	apkroottool deps <dir>

BUILD INITFS USERSPACE
	apkroottool subset-cpiogz <dir> <out> <fileglobs>
(/mkinitfs)

ifmount modloop_unmount
	if [ "$MNTDIR" ]; then
		ignore_sigs
		umount /.modloop
		remount -w
	fi


install_staged_boot
	mkdir -p "$DESTDIR"/${MEDIA:+boot/}
	mv $STAGING/* "$DESTDIR"/${MEDIA:+boot/}

install_staged_dtbs

	if [ -d "$DTBDIR" ]; then
		_opwd=$PWD
		case "$MEDIA,$FLAVOR" in
		yes,rpi*) _dtb="$DESTDIR/" ;;
		yes,*)    _dtb="$DESTDIR/boot/dtbs" ;;
		*,*)      _dtb="$DESTDIR/dtbs" ;;
		esac
		mkdir -p "$_dtb"
		_dtb=$(realpath "$_dtb")
		cd "$DTBDIR"
		find -type f \( -name "*.dtb" -o -name "*.dtbo" \) | cpio -pudm "$_dtb" 2> /dev/null
		cd "$_opwd"
	fi

ifmount modloop_remount
	if [ "$MNTDIR" ]; then
		set +e
		sync
		remount -r
		mount -o loop "$DESTDIR/$MODIMG" /.modloop
	fi

exit 0
