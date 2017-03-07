# Busybox udhcp client/server (udhcpc/udhcpd)
feature_dhcp_client_udhcp() {
	dhcp_client_enabled="true"
	dhcp_client_provider="udhcp"
	# Busybox builtin udhcp client
	return 0
}

ovl_script_dhcp_client_udhcp() {
	return 0
}


feature_dhcp_server_udhcp() {
	dhcp_server_enabled="true"
	dhcp_server_provider="udhcp"
	# Busybox builtin udhcp server
	return 0
}

ovl_script_dhcp_server_udhcp() {
	ovl_runlevel_add boot udhcpd
}
