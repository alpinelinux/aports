# vim: set ts=4:

readonly ALPINE_ROOT='/mnt/alpine'
readonly CLONE_DIR="${CLONE_DIR:-$(pwd)}"
readonly MIRROR_URI='http://dl-cdn.alpinelinux.org/alpine/edge'

# Runs commands inside the Alpine chroot.
alpine_run() {
	local user="${1:-root}"
	local cmd="${2:-sh}"

	local _sudo=
	[ "$(id -u)" -eq 0 ] || _sudo='sudo'

	$_sudo chroot "$ALPINE_ROOT" /usr/bin/env -i su -l $user \
		sh -c "cd $CLONE_DIR; $cmd"
}

die() {
	print -s1 -c1 "$@\n" 1>&2
	exit 1
}

# Marks start of named folding section for Travis and prints title.
fold_start() {
	local name="$1"
	local title="$2"

	printf "\ntravis_fold:start:$name "
	print -s1 -c6 "> $title\n"
}

# Marks end of the named folding section.
fold_end() {
	local name="$1"

	printf "travis_fold:end:$name\n"
}

# Prints formatted and colored text.
print() {
	local style=0
	local fcolor=9

	local opt; while getopts 's:c:' opt; do
		case "$opt" in
			s) style="$OPTARG";;
			c) fcolor="$OPTARG";;
		esac
	done

	shift $(( OPTIND - 1 ))
	local text="$@"

	printf "\033[${style};3${fcolor}m$text\033[0m"
}

title() {
	printf '\n'
	print -s1 -c6 "==> $@\n"
}
