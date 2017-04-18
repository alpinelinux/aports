# From multitool
checksum() {
	sha1sum | cut -f 1 -d ' '
}


# From kerneltool

# Usage: calc_path_checksums <path> <list of checksum functions...>
calc_path_checksums() { 
	local _path="$1" ; shift
	sumf=""
	for sumf in $@ ; do (cd "$_path" && find -type f -print0 | sed -e 's|\./||g' | xargs -0 -r -n 1 ${sumf}sum ) | sed -e 's|^|'"$sumf"':|g' -e 's|[[:space:]]+./|\t|g' -e 's|[[:space:]]+|\t|g' ; done | sort -b -u -k2 -k1 
	unset sumf
}

# Usage: kerneltool_calc_path_manifest <path> <prepend tag>
kerneltool_calc_path_manifest() {
	local _path="$1" _tag="$2"
	printf '#\n###### Package Directory Manifest ######\n#\n'
	printf '## Path: %s\n' "$_path"
	printf '## Tag: %s\n' "$_tag"
	calc_path_checksums "$_path" sha512 sha1 | sed -e 's|^|'"$_tag"'\t|g'
}

# Verify that all files under specified path exactly match the manifest by checksums with no extra or missing files.
# Usage: kerneltool_verify_path_manifest <path> <manifest file>
kerneltool_verify_path_manifest() {
	local _path="${1%/}" _manifest="$2"
	if file_exists "$_manifest" && dir_exists "$_path" ; then
		kerneltool_calc_path_manifest "$_path" "verify" | cat - "$_manifest" | grep -v '^#' | cut -f 2,3 | sort -b -k2 -k1 | uniq -u | grep -q '.*' || return 0
	fi
	return 1
}


calc_file_checksums() { 
	local _file="$1" ; shift
	local sumf
	( [ "${_file#/}" != "$_file" ] && cd "${_file%/*}" ; for sumf in $@ ; do ${sumf}sum "$_file" | sed -e 's|^|'"$sumf"':|g' -e 's/[[:space:]]+/\t/g' ; done ) | sort -b -u -k1
}

verify_file_checksums() { 
	local _file="$1" _sum="$2" sumf
	sumf="$(printf '%s' "$_sum" | sed -E -n -e 's|^#*[[:space:]]*([[:alnum:]]+):.*|\1|p')"
	_sum="$(printf '%s' "$_sum" | sed -E -n -e 's|^#*[[:space:]]*[[:alnum:]]+:([[:alnum:]]+)[[:space:]].*|\1|p')"
	( cd "${_file%/*}" && printf '%s\n' "$_sum" ; ${sumf}sum "$_file" | cut -d' ' -f 1 ) | uniq -u | grep -q '.*' || return 0
	msg "Checksum mismatch detected for '$_file':"
	msg2 "Expected: $_sum"
	msg2 "Received: $(${sumf}sum "$_file" | cut -d' ' -f 1)"
	return 1
}
