##
## General purpose plugins loader.
##
## Requires: 'source utils/utils-list.sh' 'var_list_alias all_plugins' 'set_all_plugins'
##
## Default plugin basename is "plugins"
##  
## Usage: 'load_plugins "<filename>" (<plugin_basenme>)' to load all plugin types and plugins from a file
##        'load_plugins "<directory>" (<plugin_basename>)' to load all plugins found in directory hierarchy.


# Main load_plugins function which wraps all the work of discovering, sourcing, and loading plugin types and plugins.
load_plugins() {
	local OIFS=$IFS ; IFS=$'\n\t '
	local target plugins_basename plugins_listname plugins_old plugins_new _plug _found

	target="$1"
	plugins_basename="${2:-plugins}"
	shift 2

	plugins_listname="all_${plugins_basename}"
	local _found=""
	local new_plugins=""

	( [ -e "$target" ] && [ -r "$target" ] ) || return 0
	target="$(realpath "$target")"

	info_func_set "load_plugins"
	if [ -f "$target" ] ; then
		msg "Discovering plugins from file '$target':"
	else
		msg "Discovering plugins under directory '$target':"
	fi

	# First run is to find all plugin types:
	plugins_old="$(getvar "$plugins_listname")"
	_load_plugins_from_target_recursive "$target" "$plugins_listname" "$plugins_basename"

	# Run all new plugin_* scripts found before loading plugins themselves.
	plugins_new="$(list_del "$plugins_old" "$(getvar "$plugins_listname")" )"
	for _plug in $plugins_new ; do
		_plug="${_plug%% }"
		_plug="${_plug## }"
		[ "${_plug}" != "${plugins_basename%% }" ] && plugin_${_plug}
	done

	# Run again to load all plugins of all discoverd types:
	_load_plugins_from_target_recursive "$target" "$plugins_listname"

	msg2 "$(printf "%s " $(getvar $plugins_listname) )"

	info_func_set ""
	IFS=$OIFS
}


# Usage: _load_plugins_from_file <file> <name of plugins list> (<function prefixes to search>)
_load_plugins_from_file() {
	local OIFS=$IFS ; IFS=$'\n\t '
	local _file _list _found
	_file="$1"
	_list="$2"
	shift 2

	_found="false"

	# Check that $_file is actually a readable file, then update _file with canonical path.
	[ -f "$_file" ] && [ -r "$_file" ] || return 1
	_file="$(realpath "$_file")"

	# Find all lines containing function definitions beginning with specified prefixes.
	local _function
	for _function in $( _load_plugins_grep_file_funcs "${_file}" $(var_list_get "$_list") $@ ) ; do
		_function="${_function%% }"
		_function="${_function## }"
		local _plugtype _pluglist _plug
		_pluglist="$(var_list_get "$_list")"
		for _plugtype in $_pluglist $@ ; do
			_plugtype="${_plugtype%% }"
			_plugtype="${_plugtype## }"

			# If plugtype is empty, continue on
			[ "${_plugtype}" ] || continue

			# If this is our first run, only load the plugin list.
			[ "$_firstrun" = "true" ] && [ "$_list" != "all_${_plugtype}" ] && continue

			# Snip '<plugin type>_' stem from function to get name current plugin.
			_plug="${_function#${_plugtype%s}_}"

			# If we didn't snip anything, function isn't for this plugin type.
			[ "$_plug" = "$_function" ] && continue

			# If we've already loaded this plugin, skip.
			var_list_has "all_${_plugtype}" "$_plug" && continue


			# Setup accessor aliases if they aren't already.
			( type get_all_${_plugtype} > /dev/null ) || var_list_alias "all_${_plugtype}"

			# Add current plugin to list for its plugin type and mark file for sourcing.
			var_list_add "all_${_plugtype}" "$_plug"
			_found="true"
		done
	done

	IFS=$OIFS
	[ "$_found" ] && . "$_file" && return 0
	return 1
}


# Usage: _load_plugins_target_recursive <target directory/file> <name of plugins list> (<function prefixes to search>)
_load_plugins_from_target_recursive() {
	local OIFS=$IFS ; IFS=$'\n\t '
	local _target _list _firstrun _found
	_target="$1"
	_list="$2"
	_found="false"
	shift 2

	_firstrun="false"
	( var_list_is_empty "$_list" ) && _firstrun="true" && var_list_add "$_list" "${_list#all_}"

	local _file
	while IFS=$'\n' read -r _file ; do
		_load_plugins_from_file "$_file" "$_list" $@  && _found="true"
	done<<-EOF
		$([ -f "$_target" ] && printf '%s\n' "$_target" ; [ -d "$_target" ] && ( _load_plugins_find_plugin_files "$_target" $(getvar "$_list") $@ ))
	EOF

	IFS=$OIFS
	[ "$_found" = "true" ] && return 0
	return 1

}



# Usage: _load_plugins_build_find_files_regex <list filename prefixes to search for>
_load_plugins_build_find_files_regex() {
	local OIFS=$IFS ; unset IFS
	printf "%s" $1 ; shift
	[ $# -gt 0 ] && printf "\|%s" $@
	IFS=$OIFS
}


# Usage: _load_plugins_find_plugin_files <target directory/file> <list of filename prefixes to search for>
_load_plugins_find_plugin_files() {
	local OIFS=$IFS ; unset IFS
	local _target
	_target="$1" ; shift
	find "$_target" -type f -regex "$_target.*/\($(IFS=$'\n\t ' _load_plugins_build_find_files_regex $(IFS=$'\n\t ' list_strip_suffix "s" $@) )\)-.*\.sh" -exec printf '%s\n' {} \; 
	IFS=$OIFS
}


# Usage: _load_plugins_build_grep_funcs_exps <list of function prefixes to search for>
_load_plugins_build_grep_funcs_exps() {
	local OIFS=$IFS ; IFS=$'\n\t '
	local _tmp
	while [ $# -gt 0 ] ; do
		_tmp="${1## }"
		_tmp="${_tmp%% }"
		shift
		printf '^%s[_[:alnum:]]+[[:space:]]*()\n' "${_tmp}"
	done
	IFS=$OIFS
}


# Usage: _load_plugins_grep_file_funcs <file to search> <list of function prefixes to search for>
_load_plugins_grep_file_funcs() {
	local OIFS=$IFS ; IFS=$'\n\t '
	local _file
	_file="$1" ; shift
	grep_file_e "$_file" -E $(_load_plugins_build_grep_funcs_exps $(list_strip_suffix "s" $@) ) | sed -n -E -e 's/^([_[:alnum:]]+).*/\1/p'
	IFS=$OIFS
}


