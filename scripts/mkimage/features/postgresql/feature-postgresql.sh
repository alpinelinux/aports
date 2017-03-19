feature_postgresql() {
	local _arg _opt _val
	local _autostart _autoload
	while [ "$1" ] ; do
		_arg="$1"
		shift
		_opt="${_arg%=*}"
		_val="${_arg#*=}"
		case "$_opt" in
			pgport )
				postgresql_conf_pgport="$_val" ;;
			pgdata )
				postgresql_conf_pgdata="$_val" ;;
			pgdump )
				postgresql_conf_pgdump="$_val" ;;
			autostart )
				_autostart="true" ;;
			autoload )
				_autoload="true" ;;
			from | src | dump_src )
				postgresql_dump_src="$_val" ;;
			to | dest | dump_dest )
				postgresql_dump_dest="$_val" ;;
			initdb_opt )
				postgresql_conf_initdb_opts="${postgresql_conf_initdb_opts $_val}" ;;
		esac
	done

	postgresql_conf_initdb_opts="${postgresql_conf_initdb_opts:---encoding=UTF8 --no-locale}"

	[ "$_autostart" = "true" ] && feature_postgresql_autostart
	[ "$_autoload" = "true" ] && feature_postgresql_autoload
	add_apks "postgresql"
	add_overlays "postgresql"
}

feature_postgresql_autostart() {
	postgresql_autostart="true"
	add_rootfs_apks "postgresql"
}

feature_postgresql_autoload() {
	if [ "$postgresql_dump_src" ] && ! file_is_readable "$postgresql_dump_src" ; then
		warning "Could not read pgsql dump file '$postgresql_dump_src'."
		return 1
	fi

	postgresql_autoload="true"
	postgresql_conf_pgdump="${postgresql_conf_pgdump:-${postgresql_dump_dest:=/var/lib/postgresql/backup/databases.pgdump}}"
	feature_postgresql_autostart
}

overlay_postgresql() {
	postgresql_dump_dest="${postgresql_dump_dest:-/var/lib/postgresql/backup/databases.pgdump}"
	_call=""
	[ "$postgresql_autostart" = "true" ] && var_list_add _call "ovl_script_postgresql_autostart"
	[ "$postgresql_autoload" = "true" ] && var_list_add _call "ovl_script_postgresql_autoload"
	[ "$postgresql_dump_src" ] && var_list_add _call "ovl_script_postgresql_dump_copy"
	[ "$postgresql_conf_pgport" ] && var_list_add _call "ovl_script_postgresql_conf"
	[ "$postgresql_conf_pgdata" ] && var_list_add _call "ovl_script_postgresql_conf"
	[ "$postgresql_conf_pgdump" ] && var_list_add _call "ovl_script_postgresql_conf"
}

# Copy the specified dump to the destination.
ovl_script_postgresql_dump_copy() {
	local _err=0
	if ! file_is_readable "$postgresql_dump_src" ; then
		warning "Could not read pgsql dump file '$postgresql_dump_src'."
		return 1
	fi

	if [ "${postgresql_dump_dest#IMAGEROOT/}" != "$postgresql_dump_dest" ] ; then
		postgresql_autoload_dest="${DESTDIR}/${postgresql_dump_dest#IMAGEROOT/}"
	elif [ "${postgresql_dump_dest#HOSTROOT/}" != "$postgresql_dump_dest" ] ; then
		postgresql_autoload_dest="${postgresql_dump_dest#HOSTROOT}"
	else
		postgresql_autoload_dest="$(ovl_get_root)/${postgresql_dump_dest}"
	fi

	ovl_fkrt_enable
	( 	mkdir_is_writable "${postgresql_dump_dest%/*}" \
		&& cp -L "$postgresql_dump_src" "$postgresql_dump_dest" \
		|| _err=1
	)
	ovl_fkrt_disable

	if [ $_err -ne 0 ] ; then
		warning "Could not copy '$postgresql_dump_src' to '$postgresql_dump_dest'."
		return 1
	fi
}

ovl_script_postgresql_autostart() {
	ovl_runlevel_add default postgresql
}

ovl_script_postgresql_autoload() {
	ovl_runlevel_add default pg-restore
}

ovl_script_postgresql_conf() {
	if [ "$postgresql_conf_pgport$postgresql_conf_pgdata$postgresql_conf_initdb_opts" ] ; then
		apk_extract_files postgresql "$(ovl_get_root)" etc/conf.d/postgresql
		ovl_conf_d_file_setting postgresql PGPORT "$postgresql_conf_pgport"
		ovl_conf_d_file_setting postgresql PGDATA "$postgresql_conf_pgdata"
		ovl_conf_d_file_setting postgresql PG_INITDB_OPTS "$postgresql_conf_initdb_opts"
	fi

	if [ "$postgresql_conf_pgdump" ] ; then
		apk_extract_files postgresql "$(ovl_get_root)" etc/conf.d/pg-restore
		ovl_conf_d_file_setting pg-restore PGDUMP "$postgresql_conf_pgdump"
	fi
}

