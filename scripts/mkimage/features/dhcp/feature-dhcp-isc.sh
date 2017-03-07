# ISC DHCP client/server (dhclient/dhcp)
feature_dhcp_client_isc() {
	dhcp_client_enabled="true"
	dhcp_client_provider="isc"
	add_apks "dhclient"
	[ "$dhcp_client_autostart" = "true" ] && add_rootfs_apks "dhclient"
}

ovl_script_dhcp_client_isc() {
	return 0
}


feature_dhcp_server_isc() {
	dhcp_server_enabled="true"
	dhcp_server_provider="isc"
	add_apks "dhcp"
	[ "$dhcp_server_autostart" = "true" ] && add_rootfs_apks "dhcp"
}

ovl_script_dhcp_server_isc() {
	ovl_runlevel_add boot dhcpd
}
