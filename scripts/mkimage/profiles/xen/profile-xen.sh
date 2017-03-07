profile_xen() {
	profile_standard
	feature_xen
	set_archs "x86_64"
	add_apks "ethtool lvm2 mdadm multipath-tools openvswitch sfdisk"
}
