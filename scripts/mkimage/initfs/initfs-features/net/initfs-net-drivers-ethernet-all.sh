initfs_net_drivers_ethernet_all() {
	return 0
}

_initfs_net_drivers_ethernet_all_modules() {
	cat <<-'EOF'
		kernel/drivers/net/ethernet/
	EOF

}

_initfs_net_drivers_ethernet_all_files() { return 0 ; }

