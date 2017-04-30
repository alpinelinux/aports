initfs_feature_net_dhcp() { return 0 ; }

_initfs_feature_net_dhcp_modules() {
	cat <<-'EOF'
		af_packet
	EOF

}

_initfs_feature_net_dhcp_pkgs() {
	cat <<-'EOF'
		busybox-initscripts
	EOF
}

_initfs_feature_net_dhcp_files() {
	cat <<-'EOF'
		/usr/share/udhcpc/default.script
	EOF
}
