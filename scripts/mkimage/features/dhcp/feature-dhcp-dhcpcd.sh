# dhcpcd DHCP client - no server
feature_dhcp_client_dhcpcd() {
	dhcp_client_enabled="true"
	dhcp_client_provider="dhcpcd"
	# dhcpcd doesn't provide a server, default to server from udhcp.
	[ "$dhcp_server_provider" = "dhcpcd" ] && dhcp_server_provider="udhcp"
	add_apks "dhcpcd"
	[ "$dhcp_client_autostart" = "true" ] && add_rootfs_apks "dhcpcd"
}

ovl_script_dhcp_client_dhcpcd() {
	ovl_runlevel_add boot dhcpcd
}
