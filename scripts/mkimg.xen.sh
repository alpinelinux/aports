build_xen() {
	apk fetch --root "$APKROOT" --stdout xen-hypervisor | tar -C "$DESTDIR" -xz boot
}

section_xen() {
	[ -n "${xen_params+set}" ] || return 0
	build_section xen $ARCH $(apk fetch --root "$APKROOT" --simulate xen-hypervisor | checksum)
}

profile_xen() {
	profile_standard
	profile_abbrev="xen"
	title="Xen"
	desc="Built-in support for Xen Hypervisor.
		Includes packages targetted at Xen usage.
		Use for Xen Dom0."
	arch="x86_64"
	kernel_addons="zfs"
	xen_params=""
	apks="$apks ethtool lvm2 mdadm multipath-tools rng-tools sfdisk xen xen-bridge xen-qemu syslinux zfs"

	local _k _a
	for _k in $kernel_flavors; do
		apks="$apks linux-$_k"
		for _a in $kernel_addons; do
			apks="$apks $_a-$_k"
		done
	done
}
