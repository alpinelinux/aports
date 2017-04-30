# grep_ -v -E -e '$1' -e '$2'
# evaluates to
# grep -v -E -e '$1' -e '$2'
grep_() {

	(unset IFS ; eval "grep $@")
}

# $cmd | grep_exps -v -E $1 $2 
# evaluates to
# $cmd | grep -v -E -e '$1' -e '$2'
pipe_grep_e() {
	local opts

	while [ "$1" != "${1#-}" ] ; do
		opts="${opts}${opts:+ }$1"
		shift
	done

	(unset IFS ; eval "grep $opts $(printf "-e '%s' " "$@")")
}


# grep_exps $filespec -v -E $1 $2 
# evaluates to:
# grep -v -E -e '$1' -e '$2' $filespec
grep_file_e() {
	local filespec opts

	filespec="$1" ; shift

	while [ "$1" != "${1#-}" ] ; do
		opts="${opts}${opts:+ }$1" ; shift
	done

	(unset IFS ; eval "grep $opts $(printf "-e '%s' " "$@") $filespec")
}


# $cmd | grep_exps -v -E $1 $2 
# evaluates to:
# $cmd | xargs -r grep -v -E -e '$1' -e '$2'
xargs_grep_e() {
	local filespec opts

	while [ "$1" != "${1#-}" ] ; do
		opts="${opts}${opts:+ }$1" ; shift
	done

	(unset IFS ; eval "xargs -r grep $opts $(printf "-e '%s' " "$@")")
}


# $cmd | xargs0_grep_exps -v -E $1 $2 
# evaluates to:
# $cmd | xargs -0 -r grep -v -E -e '$1' -e '$2'
xargs0_grep_e() {
	local filespec opts

	while [ "$1" != "${1#-}" ] ; do
		opts="${opts}${opts:+ }$1" ; shift
	done

	(unset IFS ; eval "xargs -0 -r grep $opts $(printf "-e '%s' " "$@")")
}


# Match a relative path string against a list of globs relative to the root of the path.
# Strips leading '/' from globs.
# Treats trailing '/' as '/*', matching all paths under that directory.
# Usage: fnmatch_relative_globs <relative path> <globs>...
fnmatch_relative_globs() {
	local _tgt="$1"
	shift

	local _glob
	for _glob in "$@"; do
		case "$_glob" in
			/*/) case "$_tgt" in ${_glob#/}*) : ;; *) continue ; esac ;;
			/*) case "$_tgt" in ${_glob#/}) : ;; *) continue ; esac ;;
			*/) case "$_tgt" in $_glob*) : ;; *) continue ; esac ;;
			*/*) case "$_tgt" in $_glob) : ;; *) continue ; esac ;;
			*) case "$_tgt" in */$_glob) : ;; *) continue ; esac ;;
		esac
		return 0
	done
	return 1
}
