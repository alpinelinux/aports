#!/bin/sh

_printf() {
	local fmt

	fmt="${1}"
	shift

	printf -- "${fmt}" "${@}"
}

_print() {
	_printf '%s' "${@}"
}

_printl() {
	_printf '%s\n' "${@}"
}

_printfv() {
	local fmt var val

	var="${1}"
	shift
	fmt="${1}"
	shift
	val="$(_printf "${fmt}" "${@}")"

	eval "${var}=\${val}"
}

_printv() {
	local var

	var="${1}"
	shift

	_printfv "${var}" '%s ' "${@}" &&
		eval "${var}=\${${var}% }"
}

sh_quote() {
	_print "'"
	_printl "$1" | sed "s/'/'\\\\''/g;\$s/\$/'/;"
}

_strip_leading_slash() {
	local path

	path="${1}"

	while [ "${path}" != "${path#/}" ]; do
		path="${path#/}"
	done

	_printl "${path}"
}

_strip_trailing_slash() {
	local path

	path="${1}"

	while [ "${path}" != "${path%/}" ]; do
		path="${path%/}"
	done

	_printl "${path}"
}

_save_set_args() {
	local arg

	for arg; do
		_printl "$(sh_quote "${arg}") \\"
	done
	_printl ' '
}

_tilde_expand() {
	local extra user var

	var="${1}"
	shift
	user="${1}"
	shift
	extra="$(_strip_leading_slash "${*}")" &&
		extra="${extra:+/${extra}}"

	eval "${var}=~${user}" && eval "${var}+=\"\${extra}\""
}

_basename() {
	local path

	path="$(_strip_trailing_slash "${1}")"

	_printl "${path##*/}"
}

_dirname() {
	local path

	path="$(_strip_trailing_slash "${1}")"

	_printl "${path%/*}"
}

_realpath() {
	local cmd path

	cmd="$(command -v realpath)"
	if ! path="$("${cmd:-false}" "${1}")" ; then
		path="$(cd "$(_dirname "${1}")"; pwd)"
		path+="/$(_basename "${1}")"
	fi

	_printl "${path}"
}

is_dir_empty() {( # sub-shell
	test -d "${1}" && cd "${1}" || exit 1
	for f in  .[!.]* ..?* * ; do
		test -f "${f}" && exit 2
		test -d "${f}" && exit 3
		test -e "${f}" && exit 4
	done
	exit 0
)}

_get_ident() {
	local ident who

	who="${1}"
	case "${who}" in
		COMMITTER|AUTHOR) ;;
		*) return 1 ;;
	esac

	ident="$(git var "GIT_${who}_IDENT" | awk '{ $NF=""; NF--; $NF=""; NF--; print; }')"

	_printl "${ident}"
}

get_author_id() {
	_printl "$(_get_ident AUTHOR)"
}

get_committer_id() {
	_printl "$(_get_ident COMMITTER)"
}

