build_uboot() {
	# FIXME: Fix apk-tools to extract packages directly
	local pkg pkgs="$(apk fetch  --simulate --root /tmp/timo/apkroot-armhf/ --recursive u-boot-all | sed -ne "s/^Downloading \([^0-9.]*\)\-.*$/\1/p")"
	for pkg in $pkgs; do
		[ "$pkg" == "u-boot-all" ] || apk fetch --root "$APKROOT" --stdout $pkg | tar -C "$DESTDIR" -xz usr
	done
	mkdir -p "$DESTDIR"/u-boot
	mv "$DESTDIR"/usr/sbin/update-u-boot "$DESTDIR"/usr/share/u-boot/* "$DESTDIR"/u-boot
	rm -rf "$DESTDIR"/usr
}

section_uboot() {
	[ -n "$uboot_install" ] || return 0
	build_section uboot $ARCH $(apk fetch --root "$APKROOT" --simulate --recursive u-boot-all | sort | checksum)
}

profile_uboot() {
	profile_base
	image_ext="tar.gz"
	arch="aarch64 armhf armv7"
	case "$ARCH" in
	aarch64)
		kernel_flavors="vanilla"
		kernel_addons=
		;;
	*)
		kernel_flavors="grsec"
		kernel_addons="xtables-addons"
		;;
	esac
	initfs_features="base bootchart squashfs ext2 ext3 ext4 kms mmc raid scsi usb"
	apkovl="genapkovl-dhcp.sh"
	hostname="alpine"
	uboot_install="yes"
}
