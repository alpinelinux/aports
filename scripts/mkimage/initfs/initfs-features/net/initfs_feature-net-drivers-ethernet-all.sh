initfs_feature_net_drivers_ethernet_all() { return 0 ; }

_initfs_feature_net_drivers_ethernet_all_modules() {
	cat <<-'EOF'
		kernel/drivers/net/ethernet/
	EOF
}


