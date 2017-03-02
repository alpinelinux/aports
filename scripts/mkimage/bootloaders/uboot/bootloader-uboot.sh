bootloader_uboot() {
	if [ "$1" = "disabled" ] ; then
		bootloader_uboot_enabled="false"
	else
		bootloader_uboot_enabled="true"
		bootloader_syslinux_cfg "extlinux/extlinux.conf"
	fi
}

build_bootloader_uboot() {
	# FIXME: Fix apk-tools to extract packages directly
	local pkg pkgs="$(apk fetch  --simulate --root /tmp/timo/apkroot-armhf/ --recursive u-boot-all | sed -ne "s/^Downloading \([^0-9.]*\)\-.*$/\1/p")"
	for pkg in $pkgs; do
		[ "$pkg" == "u-boot-all" ] || apk fetch --root "$APKROOT" --stdout $pkg | tar -C "$DESTDIR" -xz usr
	done
	mkdir -p "$DESTDIR"/u-boot
	mv "$DESTDIR"/usr/sbin/update-u-boot "$DESTDIR"/usr/share/u-boot/* "$DESTDIR"/u-boot
	rm -rf "$DESTDIR"/usr
}

section_bootloader_uboot() {
	[ "$bootloader_uboot_enabled" = "true" ] || return 0
	build_section bootloader_uboot $ARCH $(apk fetch --root "$APKROOT" --simulate --recursive u-boot-all | sort | checksum)
}
