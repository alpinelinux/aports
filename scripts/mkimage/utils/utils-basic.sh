##
## Basic utility functions.
##


# Implement setvar function (Why is this missing from our version of ASH?)
# Usage: setvar <varname> <value>
setvar() {
	eval "$1=\"\$2\""
}

# Implement getvar function to complement setvar
# Usage: getvar <varname>
getvar() {
	eval "printf %s \"\$$1\""
}

