bootloader_uboot() {
	if [ "$1" = "disabled" ] ; then
		bootloader_uboot_enabled="false"
	elif [ "$1" = "enabled" ] || [ "$bootloader_uboot_enabled" != "false" ] ; then
		bootloader_uboot_enabled="true"
		bootloader_extlinux_cfg "extlinux/extlinux.conf"
	fi
}

build_bootloader_uboot() {
	# FIXME: Fix apk-tools to extract packages directly
	local pkg pkgs="$($APK fetch  --simulate --root /tmp/timo/apkroot-armhf/ --recursive u-boot-all | sed -ne "s/^Downloading \([^0-9.]*\)\-.*$/\1/p")"
	for pkg in $pkgs; do
		[ "$pkg" == "u-boot-all" ] || $APK fetch --root "$APKROOT" --stdout $pkg | tar -C "$DESTDIR" -xz usr
	done
	mkdir -p "$DESTDIR"/u-boot
	mv "$DESTDIR"/usr/sbin/update-u-boot "$DESTDIR"/usr/share/u-boot/* "$DESTDIR"/u-boot
	rm -rf "$DESTDIR"/usr
}

section_bootloader_uboot() {
	[ "$bootloader_uboot_enabled" = "true" ] || return 0
	build_section bootloader_uboot $ARCH $($APK fetch --root "$APKROOT" --simulate --recursive u-boot-all | sort | checksum)
}
