# Initifs feature network.

initfs_feature_network() { return 0 ; }

_initfs_feature_network_modules() {
	cat <<-'EOF'
		kernel/drivers/net/ethernet
		kernel/drivers/net/hyperv
		kernel/drivers/net/vmxnet3
		af_packet
	EOF
}

_initfs_feature_network_pkgs() {
	cat <<-'EOF'
		busybox-initscripts
	EOF
}

_initfs_feature_network_files() {
	cat <<-'EOF'
		/usr/share/udhcpc/default.script
	EOF
}
