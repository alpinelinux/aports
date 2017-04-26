feature_ssh() {
	local _arg _opt _val

	ssh_provider="${ssh_provider:-openssh}"

	while [ "$1" ] ; do
		_arg="$1"
		shift
		_opt="${_arg%=*}"
		_val="${_arg#*=}"
		case "$_opt" in
			provider )
				case "$_val" in
					openssh | dropbear ) ssh_provider="$_val" ;;
					* ) warning "unrecognized ssh provider '$_val'" ;;
				esac ;;
			autostart )
				var_list_add ssh_autostart_providers "$ssh_provider"
				feature_ssh_autostart ;;
			generate_keys )
				var_list_add ssh_generate_keys_providers "$ssh_provider"
				feature_ssh_generate_keys ;;
		esac
	done

	[ "$ssh_provider" ] && feature_ssh_$ssh_provider
}

feature_ssh_autostart() {
	if [ "$ssh_provider" ] ; then
		feature_ssh_$ssh_provider
		feature_ssh_autostart_${ssh_provider}
		add_overlays "ssh_autostart"
	fi
}

feature_ssh_generate_keys() {
	if [ "$ssh_provider" ] ; then
		feature_ssh_$ssh_provider
		feature_ssh_generate_keys_${ssh_provider}
		add_overlays "ssh_generate_keys"
	fi
}

overlay_ssh_autostart() {
	_call=
	local p
	for p in $ssh_autostart_providers ; do
		var_list_add _call "ovl_script_ssh_autostart_${p}"
	done
}

overlay_ssh_generate_keys() {
	_call=
	local p
	for p in $ssh_generate_keys_providers ; do
		var_list_add _call "ovl_script_ssh_generate_keys_${p}"
	done
}

