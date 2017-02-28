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


##
## Functions for handling lists contained in variables.
##
## Usage: var_list_<function> <varname> [item(s)]

var_list_has() {
	local _list=$1
	shift
	local _items="$*"

	list_has "$_items" "$(getvar "$_list")"
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
	alias $1_has="var_list_has $1"
	alias $1_has_any="var_list_has_any $1"
	alias $1_has_all="var_list_has_all $1"
	alias $1_has_exactly="var_list_has_exactly $1"

	alias add_$1="var_list_add $1"
	alias del_$1="var_list_del $1"
	alias filter_$1="var_list_filter $1"
	alias get_$1="var_list_get $1"
	alias set_$1="var_list_set $1"
	alias clear_$1="var_list_clear $1"
	alias unset_$1="var_list_unset $1"
}

