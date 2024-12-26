build_uboot() {
	# FIXME: Fix apk-tools to extract packages directly
	local pkg pkgs="$(apk fetch  --simulate --root "$APKROOT" --recursive u-boot-all | sed -ne "s/^Downloading \(.*\)\-[0-9].*$/\1/p")"
	for pkg in $pkgs; do
		[ "$pkg" = "u-boot-all" ] || apk fetch --root "$APKROOT" --stdout $pkg | tar -C "$DESTDIR" -xz usr
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
	title="Generic ARM"
	desc="Has default ARM kernel.
		Includes the uboot bootloader.
		Supports armv7 and aarch64."
	image_ext="tar.gz"
	arch="aarch64 armv7"
	kernel_flavors="lts"
	kernel_addons="xtables-addons"
	initfs_features="base bootchart ext4 kms mmc nvme phy raid scsi squashfs usb"
	apkovl="genapkovl-dhcp.sh"
	hostname="alpine"
	uboot_install="yes"
}
