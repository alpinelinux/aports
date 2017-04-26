initfs_net_dhcp() {
	return 0
}

_initfs_net_dhcp_modules() {
	cat <<-'EOF'
		kernel/net/packet/af_packet.ko
	EOF

}

_initfs_net_dhcp_files() {
	cat <<-'EOF'
		/usr/share/udhcpc/default.script
	EOF
}
