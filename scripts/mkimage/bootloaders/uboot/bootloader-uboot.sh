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
	local ub_apk
	local _tmpdir="$WORKDIR"/u-boot-apks
	mkdir -p "$_tmpdir"
	_apk fetch  --recursive u-boot-all --output "$_tmpdir"
	for ub_apk in "$_tmpdir"/*.apk; do
		(tar -tzf "$ub_apk" | grep -q "^usr/" ) && tar -C "$DESTDIR" -xf "$ub_apk" usr
	done
	mkdir -p "$DESTDIR"/u-boot
	mv "$DESTDIR"/usr/sbin/update-u-boot "$DESTDIR"/usr/share/u-boot/* "$DESTDIR"/u-boot
	rm -rf "$DESTDIR"/usr
}

section_bootloader_uboot() {
	[ "$bootloader_uboot_enabled" = "true" ] || return 0
	build_section bootloader_uboot $ARCH $(_apk fetch --simulate --recursive u-boot-all | sort | checksum)
}
