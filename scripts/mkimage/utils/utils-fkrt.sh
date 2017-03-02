# fkrt fakeroot module
#
# Usage: fkrt_enable [instance], fkrt_disable, fkrt_faked_stop [instance]

fkrt_init() {

	# paths for faked and libfakeroot.so

	local fkrt_fakeroot_prefix="/usr"
	local fkrt_fakeroot_libdir="$fkrt_fakeroot_prefix/lib"
	local fkrt_fakeroot_bindir="$fkrt_fakeroot_prefix/bin"

	local fkrt_use_abs_lib_path=1
	local fkrt_lib="libfakeroot.so"

	fkrt_faked_bin="${fkrt_fakeroot_bindir}/faked"

	# make sure the preload is available
	fkrt_lib_found="no"
	local fkrt_lib_abs=""
	if [ -r "$fkrt_fakeroot_libdir/$fkrt_lib" ] ; then
		fkrt_lib_found="yes"
		fkrt_lib_abs="$fkrt_fakeroot_libdir/$fkrt_lib"
	fi

	if [ "$fkrt_lib_found" = "no" ] ; then
		warning "fkrt: preload library '$fkrt_lib' not found at ($fkrt_lib_abs) aborting."
	    return 1
	fi
	
	# Setup libs to LD_PRELOAD
	if [ $fkrt_use_abs_lib_path -ne 0 ] ; then
		fkrt_lib="$fkrt_lib_abs"
	fi

	# Prepend any existing preloaded libs if needed.
	if [ "$LD_PRELOAD" ]; then
		fkrt_ld_preload="$fkrt_lib:$LD_PRELOAD"
  	else
		fkrt_ld_preload="$fkrt_lib"
	fi

	fkrt_inst_list=""
}

fkrt_database_load_enable() {
	local _inst
	[ "$1" ] && _inst="_$1"
	local dbpipeinst="fkrt_faked_database_pipein$_inst"
	
	if [ "$fkrt_database_load" ] && [ -f "$fkrt_database_file" ] ; then
		setvar $dbpipeinst "--load < $fkrt_database_file"
	else
		warning "fkrt: database file '$fkrt_database_file' does yet not exist."
	fi
}

fkrt_database_save_enable(){
	local _inst
	[ "$1" ] && _inst="_$1"
	local dboptsinst="fkrt_faked_database_opts_$_inst"
	
	if [ "$fkrt_database_save" ] ; then
		setvar $dboptsinst "--save-file $fkrt_database_file"
		[ -p "$fkrt_database_file" ] || fkrt_wait_in_trap=1
	fi
}

fkrt_faked_mode_root() {
	local _inst
	[ "$1" ] && _inst="_$1"
	local modeinst="fkrt_faked_mode$_inst"
	
	setvar $modeinst "unknown-is-root"
}

fkrt_faked_mode_real() {
	local _inst
	[ "$1" ] && _inst="_$1"
	local modeinst="fkrt_faked_mode$_inst"
	local modeoptsinst="fkrt_faked_mode$_inst"
	
	setvar $modeinst "unknown-is-real"
	setvar $modeoptsinst "--unknown-is-real"
}

fkrt_faked_kill() {
	local _inst
	[ "$1" ] && _inst="_$1"
	local keyinst="fkrt_fakerootkey$_inst"
	local pidinst="fkrt_faked_pid$_inst"

	local mykey=$(getvar $keyinst)
	local mypid=$(getvar $pidinst)

	[ "$mypid" ] || return 0

	if [ "$fkrt_wait_in_trap" -eq 0 ]; then
		kill -s TERM $mypid 2>/dev/null
	else
		FAKEROOTKEY=$mykey LD_PRELOAD="$fkrt_lib" /bin/ls -l / >/dev/null 2>&1
		while kill -s TERM $mypid 2>/dev/null ; do
			sleep 0.1
		done
	fi
}

fkrt_cleanup() {
	local _inst
	[ "${fkrt_inst_list## }" ] || return 0
	for _inst in $fkrt_inst_list; do
		fkrt_faked_kill $_inst
	done
}

# Start insance of faked.
fkrt_faked_start() {
	local _inst
	[ "$1" ] && _inst="_$1"
	local keyinst="fkrt_fakerootkey$_inst"
	local pidinst="fkrt_faked_pid$_inst"
	local modeinst="fkrt_faked_mode$_inst"
	local modeoptsinst="fkrt_faked_mode_opts$_inst"
	local dboptsinst="fkrt_faked_database_opts_$_inst"
	local dbpipeinst="fkrt_faked_database_pipein$_inst"
	
	# Run fkrt_init if we haven't already.
	[ "$fkrt_lib_found" = "yes" ] || fkrt_init || return 1
	
	# Setup options for faked
	[ "$(getvar $modeinst)" ] || fkrt_faked_mode_root "$1"
	local fkrt_faked_opts="$(getvar $modeoptsinst) $(getvar $dboptsinst)"
	local fkrt_faked_pipein="$(getvar $dbpipeinst)"
	fkrt_wait_in_trap=0

	# Start faked for this instance, capturing key and pid, then parse them apart.
	local fkrt_key_pid="$(eval $fkrt_faked_bin $fkrt_faked_opts $fkrt_faked_pipein)"
	setvar $keyinst "$(printf %s $fkrt_key_pid | cut -d: -f1)"
	setvar $pidinst "$(printf %s $fkrt_key_pid | cut -d: -f2)"

	# Handle EXIT, INT signals and cleanup afterwards
	trap fkrt_cleanup EXIT INT
	
	# If we failed to get both a key and pid, bail.
	if [ -z "$(getvar $keyinst)" ] || [ -z "$(getvar $pidinst)" ]; then
		warning "fkrt: error while starting the 'faked' daemon."
		return 1
	fi
	fkrt_inst_list="$fkrt_inst_list ${_inst#_}"
	fkrt_inst="${_inst#_}"
}

# Stop instance of faked.
fkrt_faked_stop() {
	local _inst
	if [ "$1" ]; then
		_inst="_$1"
	elif [ "$fkrt_inst" ] ; then
		_inst="_$fkrt_inst"
	fi
	
	local keyinst="fkrt_fakerootkey$_inst"
	local pidinst="fkrt_faked_pid$_inst"
	local modeinst="fkrt_faked_mode$_inst"
	local modeoptsinst="fkrt_faked_mode_opts$_inst"
	local dboptsinst="fkrt_faked_database_opts_$_inst"
	local dbpipeinst="fkrt_faked_database_pipein$_inst"
	local mykey=$(getvar $keyinst)
	local mypid=$(getvar $pidinst)

	# Kill faked process for this instance
	fkrt_faked_kill ${_inst#_}
	
	# Remove from instance list and cleanup list
	fkrt_inst_list="${fkrt_inst_list/${_inst#_}/ }"
	fkrt_inst_list="${fkrt_inst_list//  / }"
	fkrt_inst_list="${fkrt_inst_list//  / }"
	fkrt_inst_list="${fkrt_inst_list## }"
	fkrt_inst_list="${fkrt_inst_list%% }"

	# Unset all instance variables
	unset $fkrt_inst
	unset $keyinst
	unset $pidinst
	unset $modeinst
	unset $modeoptsinst
	unset $dboptsinst
	unset $dbpipeinst
}

# Enable fkrt instance.
fkrt_enable() {
	local _inst
	if [ "$1" ]; then
		_inst="_$1"
	elif [ "$fkrt_inst" ] ; then
		_inst="_$fkrt_inst"
	fi
	
	local keyinst="fkrt_fakerootkey$_inst"
	local pidinst="fkrt_faked_pid$_inst"
	local modeinst="fkrt_faked_mode$_inst"
	
	# Disable any current fkrt before enabling another instance.
	fkrt_disable

	# If faked is not already running for this instance, start it.
	[ "$(getvar $pidinst)" ] || fkrt_faked_start "$1" || return 1
	
	# Setup exports for this instance.
	export FAKEROOTKEY="$(getvar $keyinst)"
	export FAKED_MODE="$(getvar $modeinst)"
	export LD_PRELOAD="$fkrt_ld_preload"

	fkrt_inst="${_inst#_}"
}

# Disable fkrt instance.
fkrt_disable() {
	unset FAKEROOTKEY
	unset FAKED_MODE
	unset LD_PRELOAD
	fkrt_inst=""
}


# Local Variables:
# mode: shell-script
# End:

