info_prog_set() {
	_INFO_PROG_="$*"
}

info_func_set() {
	_INFO_FUNC_="$*"
}

info_title_get() {
	printf "%s" "${_INFO_TITLE_:-${_INFO_PROG_:+:$_INFO_PROG}}"
}

# output functions
msg() {
	if [ -n "$quiet" ]; then return 0; fi
	local prompt="$GREEN>>>${NORMAL}"
	local title="$(info_title_get)"
	title="${title:+${BLUE}${title}${NORMAL}: }"
	printf "${prompt} ${title}%s\n" "$1" >&2
}

msg2() {
	[ -n "$quiet" ] && return 0
	#      ">>> %s"
	printf "    %s\n" "$1" >&2
}

warning() {
	local prompt="${YELLOW}>>> WARNING:${NORMAL}"
	local title="$(info_title_get)"
	title="${title:+${BLUE}${title}${NORMAL}: }"
	printf "${prompt} ${title}%s\n" "$1" >&2
}

warning2() {
	#      ">>> WARNING: %s\n"
	printf "             %s\n" "$1" >&2
}

error() {
	local prompt="${RED}>>> ERROR:${NORMAL}"
	local title="$(info_title_get)"
	title="${title:+${BLUE}${title}${NORMAL}: }"
	printf "${prompt} ${title}%s\n" "$1" >&2
}

error2() {
	#      ">>> ERROR:
	printf "           %s\n" "$1" >&2
}

set_xterm_title() {
	if [ "$TERM" = xterm ] && [ -n "$USE_COLORS" ]; then
		printf "\033]0;${1:-$(info_title_get)}\007" >&2
	fi
}

disable_colors() {
	NORMAL=""
	STRONG=""
	RED=""
	GREEN=""
	YELLOW=""
	BLUE=""
}

enable_colors() {
	NORMAL="\033[1;0m"
	STRONG="\033[1;1m"
	RED="\033[1;31m"
	GREEN="\033[1;32m"
	YELLOW="\033[1;33m"
	BLUE="\033[1;34m"
}

if [ -n "$USE_COLORS" ] && [ -t 1 ]; then
	enable_colors
else
	disable_colors
fi

# caller may override
cleanup() {
	return 0
}

die() {
	error "$@"
	cleanup
	exit 1
}

