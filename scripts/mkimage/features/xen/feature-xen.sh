# Xen hypervisor feature.
# Enabled at boot by default, use xen_enabled="false" to disable.

feature_xen() {
	xen_enabled=${xen_enabled:=true}
	syslinux_cfg_entry_type="xen"
	kernel_cmdline="nomodeset"
	xen_params=
	add_apks "xen"
	add_overlays "xen_dom0"
}


build_xen() {
	apk fetch --root "$APKROOT" --stdout xen-hypervisor | tar -C "$DESTDIR" -xz boot
}


section_xen() {
	[ "${xen_enabled}" = "true" ] || return 0
	build_section xen $ARCH $(apk fetch --root "$APKROOT" --simulate xen-hypervisor | checksum)
}

syslinux_cfg_entry_xen() {
	local _flavor="$1"
	local _suffix="$2"

	[ "$xen_enabled" ] || return 0

	cat <<EOF

LABEL $_flavor
	MENU LABEL Xen/Linux $_flavor
	KERNEL /boot/syslinux/mboot.c32
	APPEND /boot/xen.gz ${xen_params} --- /boot/vmlinuz$_suffix $initfs_cmdline $kernel_cmdline --- /boot/initramfs-$_flavor
EOF

}
			
