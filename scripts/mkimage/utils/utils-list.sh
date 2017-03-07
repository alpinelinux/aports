##
## Basic list handling functions
##
## Usage: list_<function> <needle(s)> <haystack>

# Is set element: "a <Element> A" Needle is an element of haystack.
list_has() {
	local needle="$1"
	shift
	local haystack="$*"
	local i

	for i in $haystack; do
		[ "$needle" != "$i" ] || return 0
	done
	return 1
}

# Is not set element: "a <Not Element> A" Needle is not an element of haystack.
list_has_not() {
	local needle="$1"
	shift
	local haystack="$*"
	local i

	for i in $haystack; do
		[ "$needle" != "$i" ] || return 1
	done
	return 0
}

# Sets intersect: "A <Intersect> B != 0" Some element in needles is also an element of haystack"
list_has_any() {
	local needles="$1"
	shift
	local haystack="$*"
	local i

	for i in $needles; do
		list_has "$i" "$@" && return 0
	done
	return 1
}

# Is subset: "A <Subset> B" Every element of needles is an element of haystack.
list_has_all() {
	local needles="$1"
	shift
	local haystack="$*"
	local i

	for i in $needles; do
		list_has "$i" "$@" || return 1
	done
	return 0
}

# Sets equal: "A = B" Elements of needles exactly equal elements of haystack.
list_has_exactly() {
	local needles="$1"
	shift
	local haystack="$*"

	list_has_all "$needles" "$*" || return 1
	list_has_all "$*" "$needles" || return 1

	return 0
}

# Sets don't intersect: "A XOR B" No element of needles matches any element of haystack.
list_has_none() {
	local needles="$1"
	shift
	local haystack="$*"

	list_has_any "$needles" "$*" || return 1

	return 0
}

# Set union: "A <Union> B" Elements in needles or haystack.
list_add() {
	local needles="$1"
	shift
	local haystack="$*"
	local i

	for i in $needles $haystack; do
		printf %s\\n "$i"
	done | sort -u | tr '\n' ' '
}

# Set difference: "A \ B" Elements in haystack which are not also elements of needles.
list_del() {
	local needles="$1"
	shift
	local haystack="$*"
	local i

	for i in $haystack ; do
			list_has_any "$i" "$needles" || printf %s\\n "$i"
	done | sort -u | tr '\n' ' '
}

# Set intersection: "A <Intersect> B" Elements in haystack which are also elements of needles.
list_filter() {
	local needles="$1"
	shift
	local haystack="$*"
	local i

	for i in $haystack ; do
			list_has_any "$i" "$needles" && printf %s\\n "$i"
	done | sort -u | tr '\n' ' '
}

# Strip leading prefix in needle from each item in list haystack
list_strip_prefix() {
	local needle i

	needle="$1"
	shift

	for i in $@ ; do
		printf_n "${i#$needle}"
	done | sort -u | tr '\n' ' '

}

# Strip trailing suffix in needle from each item in list haystack
list_strip_suffix() {
	local needle i

	needle="$1"
	shift

	for i in $@ ; do
		printf_n "${i%$needle}"
	done | sort -u | tr '\n' ' '

}

# Add leading prefix in needle from each item in list haystack
list_add_prefix() {
	local needle i

	needle="$1"
	shift

	for i in $@ ; do
		printf_n "${needle}${i}"
	done | sort -u | tr '\n' ' '

}

# Add trailing suffix in needle from each item in list haystack
list_add_suffix() {
	local needle i

	needle="$1"
	shift

	for i in $@ ; do
		printf_n "${i}{$needle}"
	done | sort -u | tr '\n' ' '

}

# Transform each item in list haystack with 'sed -E' using commands in needles.
list_transform() {
	local needles i

	needles="$1"
	shift

	for i in $@ ; do
		printf_n "$(echo ${i} | sed -E $(IFS=$'\n\t ' printf " -e '%s'" $needles))"
	done | sort -u | tr '\n' ' '

}


##
## Functions for handling lists contained in variables.
##
## Usage: var_list_<function> <varname> [item(s)]


var_list_is_empty() {
	local _list=$1
	shift
	local _val="$(getvar "$_list")"

	[ "${_val## }" ] && return 1

	return 0
}


var_list_not_empty() {
	local _list=$1
	shift
	local _val="$(getvar "$_list")"

	[ "${_val## }" ] && return 0

	return 1
}


var_list_has() {
	local _list=$1
	shift
	local _items="$*"

	list_has "$_items" "$(getvar "$_list")"
}


var_list_has_not() {
	local _list=$1
	shift
	local _items="$*"

	list_has_not "$_items" "$(getvar "$_list")"
}


var_list_has_any() {
	local _list=$1
	shift
	local _items="$*"

	list_has_any "$_items" "$(getvar "$_list")"
}


var_list_has_all() {
	local _list=$1
	shift
	local _items="$*"

	list_has_all "$_items" "$(getvar $_list)"
}


var_list_has_exactly() {
	local _list=$1
	shift
	local _items="$*"

	list_has_exactly "$_items" "$(getvar $_list)"
}


var_list_has_none() {
	local _list=$1
	shift
	local _items="$*"

	list_has_none "$_items" "$(getvar $_list)"
}


var_list_add() {
	local _list=$1
	shift
	local _items="$*"

	local _new="$(list_add "$_items" "$(getvar $_list)")"

	setvar $_list "${_new%% }"
}


var_list_del() {
	local _list=$1
	shift
	local _items="$*"

	local _new="$(list_del "$_items" "$(getvar $_list)")"

	setvar $_list "${_new%% }"
}


var_list_filter() {
	local _list=$1
	shift
	local _items="$*"

	local _new="$(list_filter "$_items" "$(getvar $_list)")"

	setvar $_list "${_new%% }"
}


var_list_get() {
	local _list=$1
	getvar "$_list"
}


var_list_get_strip_prefix() {
	local _list=$1
	list_strip_prefix "$2" $(getvar "$_list")
}


var_list_get_strip_suffix() {
	local _list=$1
	list_strip_suffix "$2" $(getvar "$_list")
}


var_list_get_add_prefix() {
	local _list=$1
	list_add_prefix "$2" $(getvar "$_list")
}


var_list_get_add_suffix() {
	local _list=$1
	list_add_suffix "$2" $(getvar "$_list")
}


var_list_get_transform() {
	local _list=$1
	list_transform "$2" $(getvar "$_list")
}


var_list_set() {
	local _list=$1
	shift
	local _items="$*"

	local _new="$(list_add "$_items")"

	setvar $_list "${_new%% }"
}

var_list_clear() {
	local _list=$1

	setvar $_list ""
}

var_list_unset() {
	local _list=$1

	unset $_list
}


##
## Set up list variable handling aliases for convenience.
##
## Usage: var_list_alias <varname>

var_list_alias() {
	alias "${1}_is_empty"="var_list_is_empty $1"
	alias "${1}_not_empty"="var_list_not_empty $1"
	alias "${1}_has"="var_list_has $1"
	alias "${1}_has_not"="var_list_has_not $1"
	alias "${1}_has_any"="var_list_has_any $1"
	alias "${1}_has_all"="var_list_has_all $1"
	alias "${1}_has_exactly"="var_list_has_exactly $1"
	alias "${1}_has_none"="var_list_has_none $1"

	alias "add_${1}"="var_list_add $1"
	alias "del_${1}"="var_list_del $1"
	alias "filter_${1}"="var_list_filter $1"
	alias "get_${1}"="var_list_get $1"
	alias "get_${1}_strip_prefix"="var_list_get_strip_prefix $1"
	alias "get_${1}_strip_suffix"="var_list_get_strip_suffix $1"
	alias "get_${1}_add_prefix"="var_list_get_add_prefix $1"
	alias "get_${1}_add_suffix"="var_list_get_add_suffix $1"
	alias "get_${1}_transform"="var_list_get_transform $1"
	alias "set_${1}"="var_list_set $1"
	alias "clear_${1}"="var_list_clear $1"
	alias "unset_${1}"="var_list_unset $1"
}

