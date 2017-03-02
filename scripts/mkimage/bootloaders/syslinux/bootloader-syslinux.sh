# syslinux bootloader plugin for x86/x86_64
bootloader_syslinux() {
	list_has $ARCH "x86 x86_64" || return 0

	if [ "$1" = "disabled" ] ; then
		bootloader_syslinux_enabled="false"
	elif [ "$1" = "enabled" ] || [ "$bootloader_syslinux_enabled" != "false" ] ; then
		bootloader_syslinux_enabled="true"
		bootloader_syslinux_cfg "extlinux/extlinux.conf"
	fi
}

# syslinux.cfg bootloader plugin.
bootloader_syslinux_cfg() {
	list_has $ARCH "x86 x86_64" || return 0
	syslinux_cfg="${1:-boot/syslinux/syslinux.cfg}"
}

# syslinux.cfg builder -- enabled as long as output file $syslinux_cfg is specified.
section_syslinux_cfg() {
	[ "$syslinux_cfg" ] || return 0
	build_section syslinux_cfg $syslinux_cfg $(syslinux_cfg_generate | checksum)
}

# Generate syslinux.cfg including menu entries for each kernel flavor.
syslinux_cfg_generate() {
	[ -z "$syslinux_serial" ] || echo "SERIAL $syslinux_serial"
	echo "TIMEOUT ${syslinux_timeout:-20}"
	echo "PROMPT ${syslinux_prompt:-1}"
	echo "DEFAULT ${kernel_flavors%% *}"

	local _f _kf
	for _f in $kernel_flavors; do
		_kf=""
		[ "$_f" = vanilla ] || _kf=-$_f
		# Use standard entry type by default
		syslinux_cfg_entry_type="${syslinux_cfg_entry_type:-standard}"
		syslinux_cfg_entry_${syslinux_cfg_entry_type} "$_f" "$_kf"
	done
}

# Standard entry for syslinux.cfg
syslinux_cfg_entry_standard() {
	local _flavor="$1"
	local _suffix="$2"

	cat <<EOF

LABEL $_flavor
	MENU LABEL Linux $_flavor
	KERNEL /boot/vmlinuz$_suffix
	INITRD /boot/initramfs-$_flavor
	DEVICETREEDIR /boot/dtbs
	APPEND $initfs_cmdline $kernel_cmdline
EOF

}

build_syslinux_cfg() {
	local _syslinux_cfg="$1"
	mkdir -p "${DESTDIR}/$(dirname $_syslinux_cfg)"
	syslinux_cfg_generate > "${DESTDIR}"/$_syslinux_cfg
}


# Build and install syslinux bootloader if enabled.
section_bootloader_syslinux() {
	[ "$bootloader_syslinux_enabled" = "true" ] || return 0
	build_section bootloader_syslinux $(apk fetch --root "$APKROOT" --simulate syslinux | sort | checksum)
}

build_bootloader_syslinux() {
	local _fn
	mkdir -p "$DESTDIR"/boot/syslinux
	apk fetch --root "$APKROOT" --stdout syslinux | tar -C "$DESTDIR" -xz usr/share/syslinux
	for _fn in isohdpfx.bin isolinux.bin ldlinux.c32 libutil.c32 libcom32.c32 mboot.c32; do
		mv "$DESTDIR"/usr/share/syslinux/$_fn "$DESTDIR"/boot/syslinux/$_fn || return 1
	done
	rm -rf "$DESTDIR"/usr
}

