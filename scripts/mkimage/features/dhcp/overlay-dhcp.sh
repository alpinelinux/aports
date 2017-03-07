overlay_dhcp_client() {
	_call=
	if [ "$dhcp_client_autostart" = "true" ] ; then
		var_list_add _call "ovl_script_dhcp_client"
		[ "${dhcp_client_provider}" ] && var_list_add _call "ovl_script_dhcp_client_${dhcp_client_provider}"
	fi
}

overlay_dhcp_server() {
	_call=
	if [ "$dhcp_server_autostart" = "true" ] ; then
		var_list_add _call "ovl_script_dhcp_server"
		[ "${dhcp_server_provider}" ] && var_list_add _call "ovl_script_dhcp_server_${dhcp_server_provider}"
	fi
}


ovl_script_dhcp_client() {

	ovl_create_file root:root 0644 /etc/network/interfaces <<-EOF
		auto lo
		iface lo inet loopback

		auto eth0
		iface eth0 inet dhcp
	EOF
}

