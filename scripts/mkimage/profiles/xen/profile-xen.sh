profile_xen() {
	profile_standard
	feature_xen
	set_arch "x86_64"
	add_apks "ethtool lvm2 mdadm multipath-tools openvswitch sfdisk"
	add_overlays "xen_dom0"
}
