##
## Basic utility functions.
##


## Variables tools:

# Implement setvar function. (Why is this missing from our version of ASH?)
# Usage: setvar <varname> <value>
setvar() {
	eval "$1=\"\$2\""
}

# Implement getvar function to complement setvar.
# Usage: getvar <varname>
getvar() {
	eval "printf %s \"\$$1\""
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
