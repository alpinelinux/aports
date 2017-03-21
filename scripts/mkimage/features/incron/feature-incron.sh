# TODO: feature_incron - Handle multiple system incrontabs and user incrontabs.
feature_incron() {
	local _arg _opt _val
	local _autostart _autoload

	while [ "$1" ] ; do
		_arg="$1"
		shift
		_opt="${_arg%=*}"
		_val="${_arg#*=}"
		case "$_opt" in
			autostart )
				_autostart="true" ;;
			incrontab | incrontab_sys* | system_incrontab )
				incron_incrontab_sys_src="$_val" ;;
			allow | users_allow )
				var_list_add incron_users_allow "${_val//,/ }" ;;
		esac
	done
	add_apks "incron"

	[ "$_autostart" = "true" ] && feature_incron_autostart

	add_overlays "incron"

	return 0
}

feature_incron_autostart() {
	add_rootfs_apks "incron"
	incron_autostart="true"
	return 0
}


overlay_incron() {
	incron_incrontab_sys_dest="${incron_incrontab_sys_dest:-/etc/incron.d}"
	_call=""
	[ "$incron_autostart" = "true" ] && var_list_add _call "ovl_script_incron_autostart"
	[ "$incron_incrontab_sys_src" ] && var_list_add _call "ovl_script_incron_incrontab_sys_copy"
	[ "$incron_users_allow" ] && var_list_add _call "ovl_script_incron_user_allow_conf"
	return 0
}

# Copy the specified dump to the destination.
ovl_script_incron_incrontab_sys_copy() {
	local _err=0
	[ -r "$incron_incrontab_sys_src" ]  \
		|| warning "Could not read pgsql dump file '$incron_incrontab_sys_src'." \
		&& return 1

	ovl_fkrt_enable
	( 	mkdir -p "${incron_incrontab_sys_dest%/*}" \
			&& cp -L "$incron_incrontab_sys_src" "$incron_incrontab_sys_dest" \
			|| _err=1
	)
	ovl_fkrt_disable

	if [ $_err -eq 0 ] ; then
		warning "Could not copy '$incron_incrontab_sys_src' to '$incron_incrontab_sys_dest'." \
		return 1
	fi
}

ovl_script_incron_autostart() {
	ovl_runlevel_add default incrond
}

ovl_script_incron_users_allow_conf() {
	ovl_create_file "root:root" "0644" "/etc/incron.allow" <<-EOF
		root$(printf_n $incron_users_allow)
	EOF
}

