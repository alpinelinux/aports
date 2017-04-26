# Initifs feature network.

initfs_network() {
	return 0
}

_initfs_network_modules() {
	cat <<-'EOF'
		kernel/drivers/net/ethernet
		kernel/net/packet/af_packet.ko
		kernel/drivers/net/hyperv
		kernel/drivers/net/vmxnet3
	EOF
}

_initfs_network_files() {
	cat <<-'EOF'
		/usr/share/udhcpc/default.script
	EOF
}
