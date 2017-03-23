##
## Basic utility functions.
##


## Variables tools:

# Implement setvar function. (Why is this missing from our version of ASH?)
# Usage: setvar <varname> <value>
setvar() { eval "$1=\"\$2\"" ; }

# Implement getvar function to complement setvar.
# Usage: getvar <varname>
getvar() { eval "printf %s \"\$$1\"" ; }

# Set the value of the variable to null.
# Usage: var_(clear/unset) <varname>
var_clear() { setvar "$1" "" ; }

# Check to see if a variable is set or not
# Usage: var_(is_set|not_set) <varname>
var_is_set() { [ "$(eval "echo \${$1+$1_is_set}")" ] && return 0 || return 1 ; }
var_not_set() { [ "$(eval "echo \${$1+$1_is_set}")" ] && return 1 || return 0 ; }

# Check to see if a variable is empty or not
# Usage: var_(is_empty|not_empty) <varname>
var_is_empty() { [ "$(eval "echo \$$1")" ] && return 0 || return 1 ; }
var_not_empty() { [ "$(eval "echo \$$1")" ] && return 1 || return 0 ; }

# Check to see if a variable equals a given value or not.
# Usage: var_(is_equal_to|not_equal_to) <varname> <value>
var_is_equal_to() { [ "$(eval "echo \$$1")" = "$2" ] && return 0 || return 1 ; }
var_not_equal_to() { [ "$(eval "echo \$$1")" = "$2" ] && return 1 || return 0 ; }

# Set a variable only if it is not set/empty.
# Usage: var_set_if(not_set|is_empty) <varname> <value>
var_set_if_not_set() { var_not_set "$1" && setvar "$1" "$2" ; }
var_set_if_is_empty() { var_is_empty "$1" && setvar "$1" "$2" ; }


##
## Set up general variable handling aliases for convenience.
##
## Usage: var_alias <varname>

var_alias() {
	alias "${1}_is_set"="var_is_set $1"
	alias "${1}_not_set"="var_is_set $1"
	alias "${1}_is_empty"="var_is_empty $1"
	alias "${1}_not_empty"="var_not_empty $1"
	alias "${1}_is_equal_to"="var_is_equal_to $1"
	alias "${1}_not_equal_to"="var_not_equal_to $1"
	alias "get_${1}"="getvar $1"
	alias "set_${1}"="setvar $1"
	alias "set_${1}_if_not_set"="var_set_if_not_set $1"
	alias "set_${1}_if_is_empty"="var_set_if_is_empty $1"
	alias "clear_${1}"="var_clear $1"
	alias "unset_${1}"="unset $1"
}


## String printing tools:

# Print null terminated string(s).
# Usage: printf_0 <string(s)>
printf_0() {
	printf "%s0" "$@"
}

# Print \n terminated string(s).
# Usage: printf_n <string(s)>
printf_n() {
	printf "%s\n" "$@"
}

# Print string(s) using printf (replace echo)
# Usage: printf_ <string(s)>
printf_() {
	printf "%s" "$@"
}

