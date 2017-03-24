
__bootloader_syslinux_onload() {
	var_alias syslinux_serial
	return 0
}

# syslinux bootloader plugin for x86/x86_64
bootloader_syslinux() {
	list_has $ARCH "x86 x86_64" || return 0

	if [ "$1" = "disabled" ] ; then
		bootloader_syslinux_enabled="false"
	elif [ "$1" = "enabled" ] || [ "$bootloader_syslinux_enabled" != "false" ] ; then
		bootloader_syslinux_enabled="true"
		bootloader_syslinux_cfg
	fi
}

# extlinux bootloader plugin for x86/x86_64
bootloader_extlinux() {
	list_has $ARCH "x86 x86_64" || return 0

	if [ "$1" = "disabled" ] ; then
		bootloader_extlinux_enabled="false"
	elif [ "$1" = "enabled" ] || [ "$bootloader_extlinux_enabled" != "false" ] ; then
		bootloader_extlinux_enabled="true"
		bootloader_extlinux_cfg
	fi
}

# isolinux bootloader plugin for x86/x86_64
bootloader_isolinux() {
	list_has $ARCH "x86 x86_64" || return 0

	if [ "$1" = "disabled" ] ; then
		bootloader_isolinux_enabled="false"
	elif [ "$1" = "enabled" ] || [ "$bootloader_isolinux_enabled" != "false" ] ; then
		bootloader_isolinux_enabled="true"
		bootloader_isolinux_cfg
	fi
}

# syslinux.cfg bootloader plugin.
bootloader_syslinux_cfg() {
	list_has $ARCH "x86 x86_64" || return 0
	syslinux_cfg="${1:-boot/syslinux/syslinux.cfg}"
}

# extlinux.cfg bootloader plugin.
bootloader_extlinux_cfg() {
	extlinux_cfg="${1:-boot/syslinux/extlinux.cfg}"
}

# isolinux.cfg bootloader plugin.
bootloader_isolinux_cfg() {
	list_has $ARCH "x86 x86_64" || return 0
	isolinux_cfg="${1:-boot/syslinux/isolinux.cfg}"
}

# syslinux.cfg builder -- enabled if any of syslinux_cfg, extlinux_cfg, or isolinux_cfg is specified.
section_syslinux_cfg() {
	[ -z "$syslinux_cfg" ] || build_section syslinux_cfg $syslinux_cfg $(syslinux_cfg_generate | checksum) || return 1
	[ -z "$extlinux_cfg" ] || build_section syslinux_cfg $extlinux_cfg $(syslinux_cfg_generate | checksum) || return 1
	[ -z "$isolinux_cfg" ] || build_section syslinux_cfg $isolinux_cfg $(syslinux_cfg_generate | checksum) || return 1
	return 0
}

# Generate syslinux.cfg including menu entries for each kernel flavor.
# TODO: syslinux_cfg - Allow configuration of default menu option.
syslinux_cfg_generate() {
	[ -z "$syslinux_serial" ] || echo "SERIAL $syslinux_serial"
	echo "TIMEOUT ${syslinux_timeout:-20}"
	echo "PROMPT ${syslinux_prompt:-1}"
	echo "DEFAULT ${kernel_flavors%% *}"

	local _f _kf
	for _f in $(get_kernel_flavors); do
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
	local _tab=$'\t'
	cat <<-EOF

		LABEL $_flavor
		${_tab}MENU LABEL Linux $_flavor
		${_tab}KERNEL /boot/vmlinuz$_suffix
		${_tab}INITRD /boot/initramfs-$_flavor
		${_tab}APPEND $(get_initfs_cmdline) $(get_kernel_cmdline)
	EOF

}


# Build a syslinux style configuration file with given name.
build_syslinux_cfg() {
	local _syslinux_cfg="$1"
	mkdir -p "${DESTDIR}/$(dirname $_syslinux_cfg)"
	syslinux_cfg_generate > "${DESTDIR}"/$_syslinux_cfg
}


# Build and install syslinux bootloader if any of syslinux, extlinux, or isolinux if enabled.
section_bootloader_syslinux() {
	[ "$bootloader_syslinux_enabled" = "true" ] || [ "$bootloader_extlinux_enabled" = "true" ] || [ "$bootloader_isolinux_enabled" = "true" ] || return 0
	build_section bootloader_syslinux $(_apk fetch --simulate syslinux | sort | checksum)
}


# Add syslinux suite to dest boot directory.
# TODO: syslinux - Add ability to configure bootloader modules.
build_bootloader_syslinux() {
	local _fn
	mkdir -p "$DESTDIR"/boot/syslinux
	_apk fetch --stdout syslinux | tar -C "$DESTDIR" -xz usr/share/syslinux
	for _fn in isohdpfx.bin isolinux.bin ldlinux.c32 libutil.c32 libcom32.c32 mboot.c32; do
		mv "$DESTDIR"/usr/share/syslinux/$_fn "$DESTDIR"/boot/syslinux/$_fn || return 1
	done
	rm -rf "$DESTDIR"/usr
}

# TODO: syslinux - Add pxelinux support.

