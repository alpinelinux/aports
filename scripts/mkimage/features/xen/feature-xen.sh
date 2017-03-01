# Xen hypervisor feature.
# Enabled at boot by default, use xen_enabled="false" to disable.

feature_xen() {
	xen_enabled=${xen_enabled:=true}
	kernel_cmdline="nomodeset"
	xen_params=""
	add_apks "xen"
	add_overlays "xen-dom0"
}


build_xen() {
	apk fetch --root "$APKROOT" --stdout xen-hypervisor | tar -C "$DESTDIR" -xz boot
}


section_xen() {
	[ "${xen_enabled}" = "true" ] || return 0
	build_section xen $ARCH $(apk fetch --root "$APKROOT" --simulate xen-hypervisor | checksum)
}

