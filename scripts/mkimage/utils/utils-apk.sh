_apk_init() {
	APK="${1:-/sbin/apk}"
	shift
	APKOPTS="${2:-$@}"
}

_apk() {
	local _apkcmd="$1"
	shift

	[ "_apkcmd" = "fetch" ] && _apkcmd="fetch -L"

	$APK $_apkcmd${APKOPTS:+ $APKOPTS}${APKROOT:+ --root "$APKROOT"}${APKARCH:+ --arch "$APKARCH"} "$@"
}

apk_pkg_full() {
	local res="$(_apk search -x "$1")"
	[ $? -eq 0 ] || return 1
	printf '%s' $res
}

apk_check_pkgs() {
	local res=$(_apk search -x $1)
	[ $? -eq 0 ] || return 1
	if [ "$res" ]; then
		echo $*
	fi
}

apk_extract_files() {
	local myapk="$1"
	local mydest="$2"
	shift 2

	apk_check_pkgs "$myapk" || ! warning "apk_extract_files - Package '$myapk' can not be foud!" || return 1
	dir_is_writable "$mydest" || ! warning "apk_extract_files - Can not write to destination: '$mydest'" || return 1
	_apk fetch --quiet "$myapk" --stdout | tar -xz -C "$mydest" "$@"
}

apk_get_apk_arch_package_version() {
	local a="$1" && file_is_readable "$a" || ! warning "Could not read '$a'!" || return 1

	local i v _pkgname="" _pkgver="" _arch=""
	set -- $(tar -x -O -f "$a" .PKGINFO | grep -e '^pkgname = ' -e '^pkgver = ' -e '^arch = ' | cut -d' ' -f 1-3 | sed -e 's/ //g')
	for i do
		v="${i#*=}"
		i="${i%%=*}"
		case "$i" in pkgname ) _pkgname="$v" ;; pkgver ) _pkgver="$v" ;; arch ) _arch="$v" ;; esac
	done

	if [ "$_arch" ] && [ "$_pkgname" ] && [ "$_pkgver" ] ; then 
		printf '%s/%s-%s' "$_arch" "$_pkgname" "$_pkgver" && return 0
	else 
		warning "Could not determine full \$arch/\$pkgname-\$pkgver string for '$a'! (got '$(printf '%s/%s-%s' "$_arch" "$_pkgname" "$_pkgver")')"
		return 1
	fi
}

apkroot_init() {
	info_func_set "apkroot_setup"
	local _arch="$1"
	local _apkroot="$2"
	shift 2

	if dir_exists "$_apkroot" && ! dir_is_writable "$_apkroot" ; then
		warning "APKROOT '$_apkroot' exists, but is not writable!"
		return 1
	elif file_exists "$_apkroot/etc/apk/arch" ; then
		_realarch="$(cat "$_apkroot/etc/apk/arch")"
		[ "$_realarch" != "$_arch" ] && warning "Arch mismatch: '$_arch' requested, but APKROOT at '$_apkroot' setup for '$_realarch'!" && return 1
		msg "Using APKROOT '$_apkroot' for arch '$_arch'."
		APKARCH="$_arch"
		APKROOT="$_apkroot"
	else
		msg "Initilizing apk repository for '$_arch' at"
		msg2 "'$_apkroot'"
		mkdir_is_writable "$_apkroot" && APKROOT="$(realpath "$_apkroot")" && [ "$APKROOT" ] || ! warning "Can not setup writable APKROOT for arch '$_arch' at '$_apkroot'!" || return 1
		APKARCH="$_arch"
		_apk add --initdb || ! warning "Could not initilize APKROOT for arch '$APKARCH' at '$APKROOT!'" || return 1
	fi

	export APKARCH APKROOT
}

apkroot_tool() {
	info_func_set "apkroot_tool"
	_arch="$APKARCH" && [ "$_arch" ] || ! warning "Called without value set in \$APKARCH!" || return 1
	_apkroot="$APKROOT" && [ "$_apkroot" ] || ! warning "Called without value set in \$APKROOT!" || return 1
	_apkkeysdir="$_apkroot/etc/apk/keys" && mkdir_is_writable "$_apkkeysdir" || ! warning "Can not write to keys directory at '$_apkkeysdir'!" || return 1
	_apkrepofile="$_apkroot/etc/apk/repositories" && mkdir_is_writable "$_apkroot/etc/apk" || !warning "Can not write to directory '$_apkroot/etc/apk'!" || return 1
	touch "$_apkrepofile" && file_is_writable "$_apkrepofile" || ! warning "Can not write to apk repositories file '$_apkrepofile'" || return 1
	
	while [ $# -gt 0 ] ; do
		case "$1" in
			--cache-dir) dir_is_writable "$2" && rm -f "$APKROOT/etc/apk/cache" && ln -sf "$2" "$APKROOT/etc/apk/cache" || warning "Can not write to cache dir '$2'!" ; shift 2 ;;
			--repository|--repo) printf '%s\n' "$2" >> "$_apkrepofile" || warning "Could not add repo '$2' to repositories file '$_apkrepofile'" ; shift 2 ;;
			--repositories-file|--repo-file) file_is_readable "$2" && cat "$2" | grep -E -v "^#" >> "$_apkrepofile" || warning "Could not add repos from file '$2' to repositories file '$_apkrepofile'!" ; shift 2 ;;
			--key-file) file_is_readable "$2" && cp -Lf "$2" "$_apkkeysdir" || warning "Can not copy key file '$2' to '$_apkkeysdir'!" ; shift 2 ;;
			--keys-dir) dir_is_readable "$2" && cp -Lf "$2"/* "$_apkkeysdir" || warning "Can not copy keys from '$2/*' to '$_apkkeysdir'!" ; shift 2 ;;
			--host-keys) dir_is_readable "/etc/apk/keys" && cp -Lf /etc/apk/keys/* "$_apkkeysdir" || warning "Can not copy host keys from '/etc/apk/keys/*' to '$_apkkeysdir'!" ; shift 1 ;;
			*) shift ;;
		esac
	done
	sort -u -o "$_apkrepofile" "$_apkrepofile"
}

apk_build_apk_manifest() {
	local _ddir="$1"
	mkdir_is_writable "$_ddir" || ! warning "Can not write to manifest directory '$_ddir'!" || return 1
	shift

	local _apkfile
	for _apkfile in $@ ; do
		_apk_manifest="$_ddir/${_apkfile##*/}.Manifest"
		file_is_readable "$_apk_manifest" || apk_extract_apk_manifest "$_apkfile" > "$_apk_manifest"
	done
}

apk_extract_apk_manifest() {
	local _apkfile="$1"
	_apk verify -q "$_apkfile" > /dev/null 2>&1 || ! warning "Could not verify '$_apkfile'!" || return 1
	local _arch_pkg_ver="$(apk_get_apk_arch_package_version "$_apkfile")"
	printf '# APK Manifest file for %s\n' "$_arch_pkg_ver"
	printf '# sha512:%s\t%s\n' "$(cat "$_apkfile" | sha512sum | cut -d' ' -f 1)" "${_apkfile##*/}" 
	printf '\n###### APK .PKGINFO ######\n'
	tar -x -O -f "$_apkfile" .PKGINFO
	printf '\n###### APK Manifest ######\n'
	zcat "$_apkfile" | apk_extract_from_tar_stream_checksums_sha1 | sort -u -k 3
}

apk_extract_from_tar_stream_checksums_sha1() {
	# Process with awk script
	awk -e 'BEGIN { paxname="NONE" ; fn="NONE"; }

		/^.PKGINFO/ { head="YES" ; }

		head=="YES" && /^arch = / { arch=$3 ; }

		head=="YES" && /^pkgname = / { pkgname=$3 ; }

		head=="YES" && /^pkgver = / { pkgver=$3 ; }

		head=="YES" && /^datahash/ { head="NO" ; }

		/PaxHeaders.[0-9]+/ { paxname=$1 ; fn=$1;
			sub("PaxHeaders.[0-9]+/","",fn) ;
		}

		/^68 APK-TOOLS.checksum.SHA1=/ { sha=$2 ;
			sub("APK-TOOLS.checksum.SHA1=", "sha1:", sha) ;
		}

		$1==fn { print "apk:" arch "/" pkgname "-" pkgver "\t" sha "\t" fn ; }
		' # End of awk script
}
