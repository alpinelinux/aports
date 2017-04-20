###
### APKROOTTOOL
###
### Build an apk root.

# Tool: apkroottool
# Commands: init setup

tool_apkroottool() { return 0 ; }
apkroottool() {
	fkrt_init
	: "${OPT_apkroot_setup_cmdline:=--repositories-file /etc/apk/repositories --host-keys --arch-keys}"
	case "$1" in
		init)	shift
			fkrt_faked_db_save_file "rt" "$1/.fkrt-db" && fkrt_enable "rt" && apkroot_init "$@" ; _ret=$?
			fkrt_cleanup
			return $_ret
			;;
		setup|manifest|index-libs|index-bins|dep-libs|dep-bins|deps|subset)
			_cmd="$1" ; shift
			file_exists "$1/.fkrt-db" && fkrt_faked_db_load_file "rt" "$1/.fkrt-db"
			fkrt_faked_db_save_file "rt" "$1/.fkrt-db" && fkrt_enable "rt" && apkroot_init "$1" || ! fkrt_cleanup || ! warning "Failed to init apkroot at '$1'!" || return 1
			shift
			case "$_cmd" in
				setup) apkroot_setup $OPT_apkroot_setup_cmdline ; _ret=$? ;;
				manifest) apkroot_manifest_installed_packages ; _ret=$? ;;
				index-libs) apkroot_manifest_index_libs ; _ret=$? ;;
				index-bins) apkroot_manifest_index_bins ; _ret=$? ;;
				dep-libs) apkroot_manifest_dep_libs ; _ret=$? ;;
				dep-bins) apkroot_manifest_dep_bins ; _ret=$? ;;
				deps) apkroot_manifest_deps ; _ret=$? ;;
				subset) apkroot_manifest_subset "$@" ; _ret=$? ;;
			esac
			fkrt_cleanup
			return $_ret
			;;
	esac
	case "$2" in
		audit|help|add|del|fix|update|info|search|upgrade|cache|version|info|fetch|verify|dot|policy|stats|--print-arch)
			file_exists "$1/.fkrt-db" && fkrt_faked_db_load_file "rt" "$1/.fkrt-db"
			fkrt_faked_db_save_file "rt" "$1/.fkrt-db" && fkrt_enable "rt" && apkroot_init "$1" && shift && _apk ${VERBOSE:+-v} $@ ; _ret=$?
			fkrt_cleanup
			return $_ret

	esac
}

