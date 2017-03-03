#
# utils-fkrt.sh libfakeroot module to be sourced in shell.
# Chris A. Giorgi <chrisgiorgi at gmail dot com>
#
# Loosely based on /usr/bin/fakeroot.
# Provides for managing multiple fakeroot instances witin shell scripts.
# Depends on: utils-basic.sh, utils-list.sh
#
# General usage:
#	fkrt_init
#	fkrt_faked_db_(load|save)_file <instance> <file>
#	fkrt_faked_mode(root|real) <instance>
#	fkrt_faked_start <instance>
#	fkrt_enable [instance]
#	fkrt_disable
#	fkrt_faked_stop <instance>
#	fkrt_cleanup
#
# Minimal usage:
#	fkrt_enable <instance>
#	<Work with fakeroot>
#	fkrt_disable
#	<Work without fakeroot>
#	fkrt_enable
#	<Work with fakeroot>
#	fkrt_cleanup


# fkrt_init - Initilize fkrt environment settings.
# Usage: 'fkrt_init'
fkrt_init() {

	# paths for faked and libfakeroot.so

	local fakeroot_prefix="/usr"
	local fakeroot_libdir="$fakeroot_prefix/lib"
	local fakeroot_bindir="$fakeroot_prefix/bin"

	# Name of fakeroot library
	local fakeroot_lib="libfakeroot.so"

	# Select absolute (set) or relative path (unset) for fakeroot library.
	local use_abs_lib_path="true"
	#unset use_abs_lib_path

	# Set fkrt_ld_preload to empty string initially.
	fkrt_ld_preload=""

	# Path to faked binary
	fkrt_faked_bin="${fakeroot_bindir}/faked"

	# Global list of active instances is empty before any are started.
	fkrt_inst_list=""

	# Current active instance.
	fkrt_inst=""

	# Global flag to deal with open named-pipes if needed (see db saving).
	fkrt_wait_in_trap=0

	# Discover fakeroot preload library is available and mark it as such if so.
	if [ -r "$fakeroot_libdir/$fakeroot_lib" ] ; then
		fakeroot_lib="${use_abs_lib_path+$fakeroot_libdir/}$fakeroot_lib"
	else
		warning "fkrt: preload library '$fakeroot_lib' not found at ($fakeroot_lib_abs) aborting."
		return 1
	fi

	# Set our LD_PRELOAD payload, prepending to any existing preload libs if needed.
	fkrt_ld_preload="$fakeroot_lib${LD_PRELOAD:+:$LD_PRELOAD}"

}


# fkrt_faked_db_load_file - Set options for faked to load state db from file.
# Usage: 'fkrt_faked_db_load_file <instance> <file>'. Run after fkrt_init before fkrt_faked_start is called.
fkrt_faked_db_load_file() {
	local _inst="${1:+_$1}"
	local mydbfile="$2"

	local dbpipeininst="fkrt_faked_db_pipein$_inst"

	# If file exists, set faked options to load it, otherwise bail with warning.
	if [ -f "$mydbfile" ] ; then
		setvar $dbpipeininst "--load < \"$mydbfile\""
	else
		warning "fkrt: database file '$mydbfile' does yet exist, not loaded."
	fi

	fkrt_inst="${_inst#_}"
}


# fkrt_faked_db_save_file - Set options for faked to save state db to file.
# Usage: 'fkrt_faked_db_save_file <instance> <file>'. Run after fkrt_init before fkrt_faked_start is called.
fkrt_faked_db_save_file(){
	local _inst="${1:+_$1}"
	local mydbfile="$2"

	local dboptsinst="fkrt_faked_db_opts_$_inst"

	# Set options for faked
	setvar $dboptsinst "--save-file \"$mydbfile\""

	# If our output file is a named pipe, tell kill handler to loop until closed.
	[ -p "$mydbfile" ] && fkrt_wait_in_trap=1

	fkrt_inst="${_inst#_}"
}


# fkrt_faked_mode_root - Set options for faked to run in "unknown-is-root" mode.
# Usage: 'fkrt_faked_mode_root <instance>'. Run after fkrt_init before fkrt_faked_start is called.
fkrt_faked_mode_root() {
	local _inst="${1:+_$1}"

	local modeinst="fkrt_faked_mode$_inst"
	local modeoptsinst="fkrt_faked_mode$_inst"
	
	setvar $modeinst "unknown-is-root"
	setvar $modeoptsinst ""

	fkrt_inst="${_inst#_}"
}


# fkrt_faked_mode_real - Set options for faked to run in "unknown-is-real" mode.
# Usage: 'fkrt_faked_mode_real <instance>'. Run after fkrt_init before fkrt_faked_start is called.
fkrt_faked_mode_real() {
	local _inst="${1:+_$1}"

	local modeinst="fkrt_faked_mode$_inst"
	local modeoptsinst="fkrt_faked_mode$_inst"

	setvar $modeinst "unknown-is-real"
	setvar $modeoptsinst "--unknown-is-real"

	fkrt_inst="${_inst#_}"
}


# fkrt_faked_start - Start a new fkrt instance and faked.
# Usage: 'fkrt_faked_start <instance>'. Run after all faked config functions are called.
fkrt_faked_start() {
	local _inst="${1:+_$1}"

	local keyinst="fkrt_fakerootkey$_inst"
	local pidinst="fkrt_faked_pid$_inst"
	local modeinst="fkrt_faked_mode$_inst"
	local modeoptsinst="fkrt_faked_mode_opts$_inst"
	local dboptsinst="fkrt_faked_db_opts_$_inst"
	local dbpipeinst="fkrt_faked_db_pipein$_inst"

	# Run fkrt_init if we haven't already.
	[ "$fkrt_ld_preload" ] || fkrt_init || return 1

	# Setup options for faked
	[ "$(getvar $modeinst)" ] || fkrt_faked_mode_root "$1"
	local faked_opts="$(getvar $modeoptsinst) $(getvar $dboptsinst)"
	local faked_pipein="$(getvar $dbpipeinst)"

	# Start faked for this instance, capturing key and pid, then parse them apart.
	local key_pid="$(eval $fkrt_faked_bin $faked_opts $faked_pipein)"
	setvar $keyinst "$(printf %s $key_pid | cut -d: -f1)"
	setvar $pidinst "$(printf %s $key_pid | cut -d: -f2)"

	# Handle EXIT, INT signals and cleanup afterwards
	trap fkrt_cleanup EXIT INT
	
	# If we failed to get both a key and pid, bail.
	if [ -z "$(getvar $keyinst)" ] || [ -z "$(getvar $pidinst)" ]; then
		warning "fkrt: error while starting the 'faked' daemon."
		return 1
	fi

	# Add this instance to the list and set as the current instance.
	var_list_add fkrt_inst_list "${_inst#_}"
	fkrt_inst="${_inst#_}"
}


# fkrt_enable - Enable given instance or last if none given. Will start instance if needed.
# Usage: 'fkrt_enable [instance]'. Run after all faked config functions are called.
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
	[ "$(getvar $pidinst)" ] || fkrt_faked_start "${_inst#_}" || return 1

	# Setup exports for this instance.
	export FAKEROOTKEY="$(getvar $keyinst)"
	export FAKED_MODE="$(getvar $modeinst)"
	export LD_PRELOAD="$fkrt_ld_preload"

	fkrt_inst="${_inst#_}"
}


# fkrt_disable - Disable fkrt instance by unsetting exported variables.
# Usage: 'fkrt_disable'. May resume using same instance with 'fkrt_enable'.
fkrt_disable() {
	unset FAKEROOTKEY
	unset FAKED_MODE
	unset LD_PRELOAD
}


# fkrt_faked_stop - Stop specified fkrt faked instance or current if none given. Removes instance from environment
# Usage: 'fkrt_faked_stop [instance]'.
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
	local dboptsinst="fkrt_faked_db_opts_$_inst"
	local dbpipeinst="fkrt_faked_db_pipein$_inst"

	# Kill faked process for this instance
	fkrt_faked_kill ${_inst#_}

	# Remove from instance list and cleanup list
	var_list_del fkrt_inst_list "${_inst#_}"

	# Unset all instance variables
	unset $keyinst
	unset $pidinst
	unset $modeinst
	unset $modeoptsinst
	unset $dboptsinst
	unset $dbpipeinst

	[ "$_inst" = "_$fkrt_inst" ] && fkrt_inst=""
}


# fkrt_faked_kill - Kill specified instance of faked, default to current $fkrt_inst.
# Usage: 'fkrt_faked_kill <instance>'.
fkrt_faked_kill() {
	local _inst
	if [ "$1" ]; then
		_inst="_$1"
	elif [ "$fkrt_inst" ] ; then
		_inst="_$fkrt_inst"
	fi

	local keyinst="fkrt_fakerootkey$_inst"
	local pidinst="fkrt_faked_pid$_inst"

	local mykey=$(getvar $keyinst)
	local mypid=$(getvar $pidinst)

	[ "$mypid" ] || return 0

	# Kill PID, handle named-pipes with loop if indicated by fkrt_wait_in_trap=1
	if [ "$fkrt_wait_in_trap" -eq 0 ]; then
		kill -s TERM $mypid 2>/dev/null
	else
		FAKEROOTKEY=$mykey LD_PRELOAD="$fkrt_ld_preload" /bin/ls -l / >/dev/null 2>&1
		while kill -s TERM $mypid 2>/dev/null ; do
			sleep 0.1
		done
	fi

	unset $pidinst
}


# fkrt_cleanup - Stop all instances in fkrt_inst_list and cleanup environment.
# Usage: 'fkrt_cleanup'
fkrt_cleanup() {
	local _inst

	# Disable any active instance.
	fkrt_disable

	# Check for actual elements in the instance list
	[ "${fkrt_inst_list## }" ] || return 0

	for _inst in $fkrt_inst_list; do
		fkrt_faked_stop $_inst
	done

	unset fkrt_inst_list
	unset "fkrt_inst"
	unset "fkrt_ld_preload"
	unset "fkrt_faked_bin"
	unset "fkrt_wait_in_trap"
}

# Local Variables:
# mode: shell-script
# End:

