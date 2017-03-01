feature_ssh() {
	local _arg _opt _val

	while [ "$1" ] ; do
		_arg="$1"
		shift
		_opt="${_arg%=*}"
		_val="${_arg#*=}"
		case "$_opt" in
			provider )
				case "$_val" in
					openssh | dropbear )
						ssh_provider="$_val"
						;;
					* )
						warning "unrecognized ssh provider '$_val'"
				esac
			;;
		esac
	done

	ssh_provider="${ssh_provider:-openssh}"
	feature_ssh_$ssh_provider
}

feature_ssh_openssh() {
	add_apks "openssh"
}

feature_ssh_dropbear() {
	add_apks "dropbear"
}

