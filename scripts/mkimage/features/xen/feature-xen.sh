# Xen hypervisor feature.
# Enabled at boot by default, use xen_enabled="false" to disable.

feature_xen() {
	xen_enabled=${xen_enabled:=true}
	xen_params=
	syslinux_cfg_entry_type="xen"
	append_kernel_cmdline "nomodeset"
	add_rootfs_apks "xen"
	add_overlays "xen_dom0"
}


build_xen() {
	$APK fetch --root "$APKROOT" --stdout xen-hypervisor | tar -C "$DESTDIR" -xz boot
}


section_xen() {
	[ "${xen_enabled}" = "true" ] || return 0
	build_section xen $ARCH $($APK fetch --root "$APKROOT" --simulate xen-hypervisor | checksum)
}

syslinux_cfg_entry_xen() {
	local _flavor="$1"
	local _suffix="$2"
	local _tab=$'\t'
	[ "$xen_enabled" ] || return 0

	cat <<-EOF

		LABEL $_flavor
		${_tab}MENU LABEL Xen/Linux $_flavor
		${_tab}KERNEL /boot/syslinux/mboot.c32
		${_tab}APPEND /boot/xen.gz ${xen_params} --- /boot/vmlinuz$_suffix $(get_initfs_cmdline) $(get_kernel_cmdline) --- /boot/initramfs-$_flavor
	EOF

}
			
