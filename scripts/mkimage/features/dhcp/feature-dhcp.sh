feature_dhcp() {
	local _arg _opt _val
	local _client="false" _server="false"
	while [ $# -gt 0 ] ; do
		_arg="$1"
		shift
		_opt="${_arg%=*}"
		_val="${_arg#*=}"
		case "$_opt" in
			provider | client_provider | server_provider )
				case "$_val" in
					dhcpcd | isc | busybox ) setval dhcp_${_opt## } $_val ;;
					* ) warning "unrecognized dhcp provider '$_val'" ;;
				esac ;;
			client )
				[ "$_val" = "autostart" ] && dhcp_client_enabled="true" dhcp_client_autostart="true"
				[ "$_val" = "noautostart" ] && dhcp_client_autostart="false"
				[ "$_val" = "disabled" ] && dhcp_client_enabled="false" dhcp_client_autostart="false" _client="false"
				[ "$_val" = "enabled" ] || [ "$dhcp_client_enabled" != "false" ] && dhcp_client_enabled="true" _client="true"
				;;
			server )
				[ "$_val" = "autostart" ] && dhcp_server_enabled="true" dhcp_server_autostart="true"
				[ "$_val" = "noautostart" ] && dhcp_server_autostart="false"
				[ "$_val" = "disabled" ] && dhcp_server_enabled="false" dhcp_server_autostart="false" _server="false"
				[ "$_val" = "enabled" ] || [ "$dhcp_server_enabled" != "false" ] && dhcp_server_enabled="true" _server="true"

				[ "$_val" = "disabled" ] && dhcp_server_enabled="false" _server="false"
				[ "$_val" = "enabled" ] || [ "$dhcp_server_enabled" != "false" ] && dhcp_server_enabled="true" _server="true"
				;;
		esac
	done
	
	# Enable both client and server features if neither or both was specified, but not if only one was.
	[ "$_client" = "$_server" ] && _client="true" _server="true"

	# Default to udhcp for the provider if none specified
	dhcp_provider="${dhcp_provider:-udhcp}"
	[ "$_client" = "true" ] && feature_dhcp_client_${dhcp_client_provider:=$dhcp_provider} 
	[ "$_server" = "ture" ] && feature_dhcp_server_${dhcp_server_provider:=$dhcp_provider}
}

# Features to include dhcp client or server independently
feature_dhcp_client() {
	local _opts
	while [ $# -gt 0 ] ; do
		_arg="$1"
		case "$_arg" in
			provider* ) _opts="$_opts client_$_arg" ;;
			enabled ) _opts="$_opts client=enabled" ;;
			disabled ) _opts="$_opts client=disabled" ;;
			autostart ) _opts="$_opts client=autostart" ;;
			noautostart ) _opts="$_opts client=noautostart" ;;
		esac
	done
	feature_dhcp client "$_opts"
}

feature_dhcp_server() {
	local _opts
	while [ $# -gt 0 ] ; do
		_arg="$1"
		case "$_arg" in
			provider* ) _opts="$_opts server_$_arg" ;;
			enabled ) _opts="$_opts server=enabled" ;;
			disabled ) _opts="$_opts server=disabled" ;;
			autostart ) _opts="$_opts server=autostart" ;;
			noautostart ) _opts="$_opts server=noautostart" ;;
		esac
	done
	feature_dhcp server "$_opts"
}


# dhcpcd DHCP client - no server
feature_dhcp_client_dhcpcd() {
	dhcp_client_enabled="true"
	dhcp_client_provider="dhcpcd"
	# dhcpcd doesn't provide a server, default to server from udhcp.
	[ "$dhcp_server_provider" = "dhcpcd" ] && dhcp_server_provider="udhcp"
	add_apks "dhcpcd"
	[ "$dhcp_client_autostart" = "true" ] && add_rootfs_apks "dhcpcd"
}


# ISC DHCP client/server (dhclient/dhcp)
feature_dhcp_client_isc() {
	dhcp_client_enabled="true"
	dhcp_client_provider="isc"
	add_apks "dhclient"
	[ "$dhcp_client_autostart" = "true" ] && add_rootfs_apks "dhclient"
}

feature_dhcp_server_isc() {
	dhcp_server_enabled="true"
	dhcp_server_provider="isc"
	add_apks "dhcp"
	[ "$dhcp_server_autostart" = "true" ] && add_rootfs_apks "dhcp"
}


# Busybox udhcp client/server (udhcpc/udhcpd)
feature_dhcp_client_udhcp() {
	dhcp_client_enabled="true"
	dhcp_client_provider="udhcp"
	# Busybox builtin udhcp client
	return 0
}

feature_dhcp_server_udhcp() {
	dhcp_server_enabled="true"
	dhcp_server_provider="udhcp"
	# Busybox builtin udhcp server
	return 0
}
