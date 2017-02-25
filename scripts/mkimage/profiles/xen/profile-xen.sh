profile_xen() {
	profile_standard
	feature_xen
	arch="x86_64"
	apks="$apks ethtool lvm2 mdadm multipath-tools openvswitch sfdisk"
}
