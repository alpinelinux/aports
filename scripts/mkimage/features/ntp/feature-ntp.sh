feature_ntp() {
	local _arg _opt _val

	while [ "$1" ] ; do
		_arg="$1"
		shift
		_opt="${_arg%=*}"
		_val="${_arg#*=}"
		case "$_opt" in
			provider )
				case "$_val" in
					chrony | openntpd | sntpc )
						ntp_provider="$_val"
						;;
					* )
						warning "unrecognized ntp provider '$_val'"
				esac
			;;
		esac
	done

	ntp_provider="${ntp_provider:-chrony}"
	feature_ntp_$ntp_provider
}

feature_ntp_chrony() {
	add_apks "chrony"
}

feature_ntp_openntpd() {
	add_apks "openntpd"
}

feature_ntp_sntpc() {
	add_apks "sntpc"
}

