##
## APK Utilities
##

# Usage: _apk_init <apk binary> <apk options>
_apk_init() {
	APK="${1:-/sbin/apk}"
	shift
	APKOPTS="$*"
}


# Usage: _apk <apk command> <apk args>
_apk() {
	local _apkcmd="$1"
	shift

	#[ "$_apkcmd" = "fetch" ] && _apkcmd="fetch -L"

	$APK $_apkcmd${APKOPTS:+ $APKOPTS}${APKROOT:+ --root "$APKROOT"}${APKARCH:+ --arch "$APKARCH"} "$@"
}


# Usage: apk_pkg_full <apk pkgname>
apk_pkg_full() {
	local res="$(_apk search -x "$1")"
	[ $? -eq 0 ] && [ "$res" ] || res="$(_apk search -x "${1%-*-r*}")"
	[ $? -eq 0 ] || return 1
	printf '%s' $res
}


# Usage: apk_check_pkgs <apk pkgname>...
apk_check_pkgs() {
	local res=$(_apk search -x $@)
	[ $? -eq 0 ] || return 1
	if [ "$res" ]; then
		echo $*
	fi
}

# Fetch apk for specified package(s) to specified destination dir.
# Usage: kerneltool_apk_fetch <destination dir> <package>
apk_fetch() {
	local _ddir="$1"
	shift

	local _p _pkg _pkgname _pkgfile
	for _p in "$@" ; do
		case "$_p" in 
			/*/*.apk | ./*/*.apk | ../*/*.apk )
				file_exists "$_p" && _apk verify "$_p" || ! warning "Could not verify requested apk '$_p'!" || return 1
				_pkgfile="$_ddir/$_p"
				cp -f "$_p" "$_pkgfile"

				;;
			*.apk )
				_pkgfile="$_ddir/$_p"
				file_exists "$_pkgfile" && _apk verify "$_pkgfile" || ! warning "Could not verify apk '$_pkgfile'!" || return 1
				;;
			*)
				_pkg="$(apk_pkg_full "$_p")"
				_pkgname="${_pkg%-*-r*}"
				_pkgfile="$_ddir/$_pkg.apk"
				[ "$_p" = "$_pkgname" ] || [ "$_p" = "$_pkg" ] || "$_p" = "$_pkgfile" || ! warning "Requested package '$_p' but search found '$_pkg' instead!" || return 1
				local _a_
				for _a_ in 1 2 ; do
					if ! file_exists "$_pkgfile" ; then
						_fetched="$(_apk fetch -o "$_ddir" "$_pkgname" | cut -d' ' -f2)"
						if [ "$_fetched" != "$_pkg" ] ; then rm -f "$_ddir/$_fetched.apk" ; warning "Requested '$_pkg' but fetch found '$_fetched' instead!" ; return 1 ; fi
					fi
					_apk verify "$_pkgfile" && break
					rm -f "$_pkgfile"
					[ $_a_ -gt 1 ] && return 1
				done
				;;
		esac


	done

}

# Extract all or specified from .apk file to destination directory.
# Usage: apk_extract <apk file> <destination dir> [<paths to extract>...]
apk_extract() {
	local _apkfile="$1" _ddir="$2"
	shift 2
	_arch="${OPT_arch:-${APKARCH:-"$(_apk --print-arch)"}}"

	_archpkgver="$(apk_get_apk_arch_package_version "$_apkfile")"
	local _apkarch="${_archpkgver%%/*}"
	[ "$_arch" != "$_apkarch" ] && warning "Arch mismatch detected: '$_arch' requested but '$_apkarch' found in '$_apkfile'!" && return 1
	file_is_readable "$_apkfile" || ! warning "Could not read '$_apkfile' to extract!" || return 1
	_apk verify ${VERBOSE:--q} "$_apkfile" || ! warning "Could not verify '$_apkfile'!" || return 1

	_found="$( tar -tz -f "$_apkfile" | sed -n -E $([ "$1" ] && printf '-e s|^(%s).*|\\1|gp ' $@ || printf '1,$ p') | sort -u )"
	! [ "$_found" ] && msg "No $([ "$1" ] && printf ''\''%s'\'' ' $@ || printf 'files ')found in '$_apkfile', nothing extracted." && return 0
 
	mkdir_is_writable "$_ddir" || ! warning "Could not create output directory '$_ddir' to extract '$_apkfile'!" || return 1
	tar -C "$_ddir" -x -f "$_apkfile" $_found || ! warning "Could not extract files${_found+" in '$_found'"} from '$_apkfile'!" || return 1
	msg "Extracted $(printf ''\''%s'\'' ' $_foundi | tr -s ' \n\t' ' ')from '$_apkfile.'"

	return 0
}


# Usage: apk_extract_files <apk pkgname> <dest dir> [<file>...]
apk_extract_files() {
	local _pkgname="$1"
	local _ddir="$2"
	shift 2

	apk_check_pkgs "$_pkgname" || ! warning "apk_extract_files - Package '$_pkgname' can not be foud!" || return 1
	dir_is_writable "$_ddir" || ! warning "apk_extract_files - Can not write to destination: '$_ddir'" || return 1
	_apk fetch --quiet "$_pkgname" --stdout | tar -xz -C "$_ddir" "$@"
}

# Usage: apk_get_apk_arch_package_version <apkfile>
apk_get_apk_arch_package_version() {
	local a="$1" && file_is_readable "$a" || ! warning "Could not read '$a'!" || return 1

	local i v _pkgname="" _pkgver="" _arch=""
	set -- $(tar -x -O -f "$a" .PKGINFO | grep -e '^pkgname = ' -e '^pkgver = ' -e '^arch = ' | cut -d' ' -f 1-3 | sed -e 's/ //g')
	for i do
		v="${i#*=}"
		i="${i%%=*}"
		case "$i" in  pkgname) _pkgname="$v" ;;  pkgver) _pkgver="$v" ;;  arch) _arch="$v" ;; esac
	done

	if [ "$_arch" ] && [ "$_pkgname" ] && [ "$_pkgver" ] ; then 
		printf '%s/%s-%s' "$_arch" "$_pkgname" "$_pkgver" && return 0
	else 
		warning "Could not determine full \$arch/\$pkgname-\$pkgver string for '$a'! (got '$(printf '%s/%s-%s' "$_arch" "$_pkgname" "$_pkgver")')"
		return 1
	fi
}


# Usage: apk_build_apk_manifest <manifest dest dir> <apkfile>...
apk_build_apk_manifest() {
	local _ddir="$1"
	mkdir_is_writable "$_ddir" || ! warning "Can not write to manifest directory '$_ddir'!" || return 1
	shift

	local _apkfile _apk_manifest
	for _apkfile in $@ ; do
		_apk_manifest="$_ddir/${_apkfile##*/}.Manifest"
		file_is_readable "$_apk_manifest" || apk_extract_apk_manifest "$_apkfile" > "$_apk_manifest"
	done
}


# Usage: apk_extract_apk_manifest <apkfile>
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

# Usage: (uncompressed apk tarfile) | apk_extract_from_tar_stream_checksums_sha1
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

# Usage apkroot_init <root> [<arch>]
apkroot_init() {
	info_func_set "apkroot_setup"
	local _apkroot="$1"
	local _arch="$2"
	shift 2

	if dir_exists "$_apkroot" && ! dir_is_writable "$_apkroot" ; then
		warning "APKROOT '$_apkroot' exists, but is not writable!"
		return 1
	elif file_exists "$_apkroot/etc/apk/arch" ; then
		local _realarch="$(cat "$_apkroot/etc/apk/arch")"
		[ "$_arch" ] || _arch="$_realarch"
		[ "$_realarch" != "$_arch" ] && warning "Arch mismatch: '$_arch' requested, but APKROOT at '$_apkroot' setup for '$_realarch'!" && return 1
		msg "Using APKROOT '$_apkroot' for arch '$_arch'."
		APKARCH="$_arch"
		APKROOT="$_apkroot"
	else
		[ "$_arch" ] || _arch="$(_apk --print-arch)"
		msg "Initilizing apk repository for '$_arch' at"
		msg2 "'$_apkroot'"
		mkdir_is_writable "$_apkroot" && APKROOT="$(realpath "$_apkroot")" && [ "$APKROOT" ] || ! warning "Can not setup writable APKROOT for arch '$_arch' at '$_apkroot'!" || return 1
		APKARCH="$_arch"
		_apk add --initdb || ! warning "Could not initilize APKROOT for arch '$APKARCH' at '$APKROOT!'" || return 1
	fi

	export APKARCH APKROOT
}

# Usage apkroot_setup <options>...
apkroot_setup() {
	info_func_set "apkroot_setup"
	local _arch="$APKARCH" && [ "$_arch" ] || ! warning "Called without value set in \$APKARCH!" || return 1
	local _apkroot="$APKROOT" && [ "$_apkroot" ] || ! warning "Called without value set in \$APKROOT!" || return 1
	local _apkkeysdir="$_apkroot/etc/apk/keys" && mkdir_is_writable "$_apkkeysdir" || ! warning "Can not write to keys directory at '$_apkkeysdir'!" || return 1
	local _apkrepofile="$_apkroot/etc/apk/repositories" && mkdir_is_writable "$_apkroot/etc/apk" || !warning "Can not write to directory '$_apkroot/etc/apk'!" || return 1
	touch "$_apkrepofile" && file_is_writable "$_apkrepofile" || ! warning "Can not write to apk repositories file '$_apkrepofile'" || return 1
	
	while [ $# -gt 0 ] ; do
		case "$1" in
			--cache-dir) dir_is_writable "$2" && rm -f "$APKROOT/etc/apk/cache" && ln -sf "$2" "$APKROOT/etc/apk/cache" || warning "Can not write to cache dir '$2'!" ; shift 2 ;;
			--repository|--repo) printf '%s\n' "$2" >> "$_apkrepofile" || warning "Could not add repo '$2' to repositories file '$_apkrepofile'" ; shift 2 ;;
			--repositories-file|--repo-file) file_is_readable "$2" && cat "$2" | grep -E -v "^#" >> "$_apkrepofile" || warning "Could not add repos from file '$2' to repositories file '$_apkrepofile'!" ; shift 2 ;;
			--key-file) file_is_readable "$2" && cp -Lf "$2" "$_apkkeysdir" || warning "Can not copy key file '$2' to '$_apkkeysdir'!" ; shift 2 ;;
			--keys-dir) dir_is_readable "$2" && cp -Lf "$2"/* "$_apkkeysdir" || warning "Can not copy keys from '$2/*' to '$_apkkeysdir'!" ; shift 2 ;;
			--host-keys) dir_is_readable "/etc/apk/keys" && cp -Lf /etc/apk/keys/* "$_apkkeysdir" || warning "Can not copy host keys from '/etc/apk/keys/*' to '$_apkkeysdir'!" ; shift 1 ;;
			--arch-keys) dir_is_readable "/usr/share/apk/keys/$_arch" && cp -Lf "/usr/share/apk/keys/$_arch"/* "$_apkkeysdir" \
					|| warning "Can not copy arch keys from '/usr/share/apk/keys/$_arch/*' to '$_apkkeysdir'!"
				shift 1
				;;
			*) shift ;;
		esac
	done
	sort -u -o "$_apkrepofile" "$_apkrepofile"
}


# Usage: apkroot_manifest_installed_packages
apkroot_manifest_installed_packages() {
	local _arch="$APKARCH" && [ "$_arch" ] || ! warning "Called without value set in \$APKARCH!" || return 1
	local _apkroot="$APKROOT" && [ "$_apkroot" ] || ! warning "Called without value set in \$APKROOT!" || return 1

	local _line _pkgname _file _contains _pkg
	_apk info -v 2> /dev/null | while read -r _line ; do
		_pkgname="${_line%-*-*}"
		_apk info -L "$_pkgname" 2> /dev/null
	done | while read -r _file _contains ; do
		if [ "$_contains" ] ; then _pkg="$_file" ; continue ; fi
		_file="$_apkroot/$_file"
		if [ "$_file" ] && [ -e "$_file" ] && [ -L "$_file" ] ; then printf 'pkg:%s/%s\tlinkto:%s\t%s\n' "$_arch" "$_pkg" $(readlink -n "$_file") "$_file"
		elif [ "$_file" ] && [ -e "$_file" ] && [ -f "$_file" ] ; then printf 'pkg:%s/%s\tsha512:' "$_arch" "$_pkg" && sha512sum "$_file"
		fi
	done | sed -e 's|[[:space:]]*[[:space:]]'"$_apkroot"'/|\t|g' > "$_apkroot/.Manifest-apk-installed"
}


# Usage: apkroot_manifest_index_libs
apkroot_manifest_index_libs() {
	local _arch="$APKARCH" && [ "$_arch" ] || ! warning "Called without value set in \$APKARCH!" || return 1
	local _apkroot="$APKROOT" && [ "$_apkroot" ] || ! warning "Called without value set in \$APKROOT!" || return 1
	local _pkgfield _sumfield _filefield
	file_exists "$_apkroot/.Manifest-apk-installed" || apkroot_manifest_installed_packages
	cat "$_apkroot/.Manifest-apk-installed" | while read -r _pkgfield _sumfield _filefield ; do
		case "$_filefield" in
			*.so|*.so.*) printf 'libso:%s:%s:%s\t%s\t%s\t%s\n' "${_filefield##*/}" "$_arch" "$_sumfield" "$_pkgfield" "$_sumfield" "$_filefield" ;;
			*.a) printf 'liba:%s:%s:%s\t%s\t%s\t%s' "${_filefield##*/}" "$_arch" "$_sumfield" "$_pkgfield" "$_sumfield" "$_filefield" ;;
			*.rlib) printf 'librust:%s:%s:%s\t%s\t%s\t%s' "${_filefield##*/}" "$_arch" "$_sumfield" "$_pkgfield" "$_sumfield" "$_filefield" ;;
			*) continue ;;
		esac
	done > "$_apkroot/.libs.INDEX"
}


# Usage: apkroot_manifest_index_bins
apkroot_manifest_index_bins() {
	local _arch="$APKARCH" && [ "$_arch" ] || ! warning "Called without value set in \$APKARCH!" || return 1
	local _apkroot="$APKROOT" && [ "$_apkroot" ] || ! warning "Called without value set in \$APKROOT!" || return 1
	file_exists "$_apkroot/.Manifest-apk-installed" || apkroot_manifest_installed_packages
	local _pkgfield _sumfield _filefield _file _link _type
	cat "$_apkroot/.Manifest-apk-installed" | grep -v -e '\.so$' -e '\.so\..*$' -e '\.a$' | while read -r _pkgfield _sumfield _filefield ; do
		_file="$_apkroot/$_filefield"

		# Resolve links and check target so we can include link in index if it hits.
		_link="${_sumfield%linkto:}"
		if [ "$_link" != "$_sumfield" ] ; then
			[ -e "$_file" ] && [ -L "$_file" ] && [ "$(readlink -n "$_file")" = "$_link" ] || ! warning "Did not find symlink '$_file' -> '$_link' as expected per Manifest!" || continue
			case "$_link" in /* ) _link="$_apkroot$_link" ;; *) _link="$_apkroot/${_filefield%/*}/$_link" ;; esac
			[ -e "$_link" ] || ! warning "Symlink target for '$_file' not found at '$_link'!" || continue
			_file="$_link"
		fi

		# Check only executable files, deterimine scripts by their shebang, otherwise check that objdump doesn't fail.
		if [ -e "$_file" ] && [ -f "$_file" ] && [ -x "$_file" ] ; then
			if head -n 1 "$_file" | grep -q "^#!" ; then _type="script"
			elif objdump -p "$_file" > /dev/null 2>&1 ; then _type="bin"
			else _type="" ; fi
			[ "$_type" ] && printf '%s:%s:%s:%s\t%s\t%s\t%s\n' "$_type" "${_filefield#/}" "$_arch" "$_sumfield" "$_pkgfield" "$_sumfield" "$_filefield"
		fi
	done > "$_apkroot/.bins.INDEX"
}


# Usage: (index with unindexed deps) | _manifest_deps_resolve_index [<additional index/deps files>...]
_manifest_deps_resolve_index() {
	awk -e '
		NF>0	{ split($1,f,":") ; idx[f[2]]=$1 ; idx[$1]=$1; deps[$1]=$0}
		END {
			for (i in deps) {
				split(deps[i], tmp)
				ts=tmp[1] ; delete tmp[1]
				for( n in tmp ) { if (tmp[n]!="") { ts=ts "\t" idx[tmp[n]] } }
				print ts
			}
		}
	' "$@"
}


# Usage: (index with indexed deps) | _manifest_deps_resolve_recurse [<additional deps files>...]
_manifest_deps_resolve_recurse() {
	awk -e '
		NF>0	{ split($1,f,":") ; ideps[$1]=$0}
		function alldeps(ad,d, sd,   td, t) {
			split(ad[d], td)
			for (t in td) { sd[td[t]]=td[t] ; if (td[t] !=d) { alldeps(ad, td[t], sd) } }
		}
		END {
			for( i in ideps) {
				split("",all)
				alldeps(ideps, i, all)
				printf i ; for( j in all ) { if(all[j] != "" && i!=j) { printf("\t%s", j) } } ; printf "\n"
			}
		}
	' "$@"
}


# Usage: apkroot_manifest_dep_libs
apkroot_manifest_dep_libs() {
	local _arch="$APKARCH" && [ "$_arch" ] || ! warning "Called without value set in \$APKARCH!" || return 1
	local _apkroot="$APKROOT" && [ "$_apkroot" ] || ! warning "Called without value set in \$APKROOT!" || return 1
	file_exists "$_apkroot/.libs.INDEX" || apkroot_manifest_index_libs
	local _indexfield _pkgfield _sumfield _filefield _file _link
	cat "$_apkroot/.libs.INDEX" | while read -r _indexfield _pkgfield _sumfield _filefield ; do
		_file="$_apkroot/$_filefield"
		# Resolve links and check target so we can include link target in deps.
		_link="${_sumfield##*linkto:}"
		if [ "$_link" != "$_sumfield" ] ; then
			[ -e "$_file" ] && [ -L "$_file" ] && [ "$(readlink -n "$_file")" = "$_link" ] || ! warning "Did not find symlink '$_file' -> '$_link' as expected per Manifest!" || continue
			case "$_link" in /* ) _link="$_apkroot$_link" ;; *) _link="$_apkroot/${_filefield%/*}/$_link" ;; esac
			[ -e "$_link" ] || ! warning "Symlink target for '$_file' not found at '$_link'!" || continue
			[ "${_file##*/}" = "${_link##*/}" ] || printf '%s\t%s\n' "$_indexfield" "${_link##*/}"
			continue
		else
			printf '%s\t%s\n' "$_indexfield" "$(objdump -p "$_file" 2> /dev/null | sed -n -E -e 's/[[:space:]]*NEEDED[[:space:]]+(.*$)/\1/gp' -e 's/[[:space:]]+//g' | tr -s ' \n\t' '\t')"
		fi
	done | _manifest_deps_resolve_index | _manifest_deps_resolve_recurse | sort -t':' -k2 -u > "$_apkroot/.libs.DEPS"
}


# Usage: apkroot_manifest_dep_bins
apkroot_manifest_dep_bins() {
	local _arch="$APKARCH" && [ "$_arch" ] || ! warning "Called without value set in \$APKARCH!" || return 1
	local _apkroot="$APKROOT" && [ "$_apkroot" ] || ! warning "Called without value set in \$APKROOT!" || return 1
	file_exists "$_apkroot/.bins.INDEX" || apkroot_manifest_index_bins
	file_exists "$_apkroot/.libs.DEPS" || apkroot_manifest_dep_libs
	local _indexfield _pkgfield _sumfield _filefield _file _link
	cat "$_apkroot/.bins.INDEX" | while read -r _indexfield _pkgfield _sumfield _filefield ; do
		_file="$_apkroot/$_filefield"
		# Resolve links and check target so we can include link target in deps.
		_link="${_sumfield##*linkto:}"
		if [ "$_link" != "$_sumfield" ] ; then
			[ -e "$_file" ] && [ -L "$_file" ] && [ "$(readlink -n "$_file")" = "$_link" ] || ! warning "Did not find symlink '$_file' -> '$_link' as expected per Manifest!" || continue
			case "$_link" in /* ) _link="$_apkroot$_link" ;; *) _link="$_apkroot/${_filefield%/*}/$_link" ;; esac
			[ -e "$_link" ] || ! warning "Symlink target for '$_file' not found at '$_link'!" || continue
			printf '%s\t%s\n' "$_indexfield" "${_link#$_apkroot/}"
			continue
		else
			case "$_indexfield" in
				script:*) printf '%s\t%s\n' "$_indexfield" "$(head -n 1 "$_file" | sed -n -E -e 's|^#!/?([^[[:space:]]*)[[:space:]]*.*|\1|p' -e 's/^\t*$//g' )" ;;
				bin:*) printf '%s\t%s\n' "$_indexfield" "$(objdump -p "$_file" 2> /dev/null | sed -n -E -e 's/^[[:space:]]*NEEDED[[:space:]]+(.*$)/\1/gp' -e 's/[[:space:]]+/\t/g' | tr -s ' \n\t' '\t')" ;;
			esac
		fi
	done | _manifest_deps_resolve_index "$_apkroot/.libs.DEPS" - | _manifest_deps_resolve_recurse | grep -e '^bin' -e '^script' | sort -t':' -k2 -u > "$_apkroot/.bins.DEPS"
}


# Usage: apkroot_manifest_deps
apkroot_manifest_deps() {
	local _arch="$APKARCH" && [ "$_arch" ] || ! warning "Called without value set in \$APKARCH!" || return 1
	local _apkroot="$APKROOT" && [ "$_apkroot" ] || ! warning "Called without value set in \$APKROOT!" || return 1
	local _f
	for _f in ".Manifest-apk-installed" ".libs.INDEX" ".bins.INDEX" ".libs.DEPS" ".bins.DEPS" ; do
		file_exists "$_apkroot/$_f" && rm -f "$_apkroot/$_f"
	done
	apkroot_manifest_index_libs
	apkroot_manifest_index_bins
	apkroot_manifest_dep_libs
	apkroot_manifest_dep_bins
}


# Usage: apkroot_manifest_subset <globs>...
apkroot_manifest_subset_deps() {
	local _arch="$APKARCH" && [ "$_arch" ] || ! warning "Called without value set in \$APKARCH!" || return 1
	local _apkroot="$APKROOT" && [ "$_apkroot" ] || ! warning "Called without value set in \$APKROOT!" || return 1
	local _line _tgt
	file_exists "$_apkroot/.libs.DEPS" && file_exists "$_apkroot/.bins.DEPS" || apkroot_manifest_deps
	cat "$_apkroot/.libs.DEPS" "$_apkroot/.bins.DEPS" | while read -r _line ; do
		for _glob in "$@"; do
			_tgt="${_line#*:}" && _tgt="${_tgt%%:*}"
			fnmatch_relative_globs "$_tgt" "$@" && printf '%s\n' "$_line"
		done
	done | sort -t':' -k2 -u
}


# Usage: apkroot_manifest_subset <globs>...
apkroot_manifest_subset() {
	local _arch="$APKARCH" && [ "$_arch" ] || ! warning "Called without value set in \$APKARCH!" || return 1
	local _apkroot="$APKROOT" && [ "$_apkroot" ] || ! warning "Called without value set in \$APKROOT!" || return 1
	file_exists "$_apkroot/.libs.DEPS" && file_exists "$_apkroot/.bins.DEPS" || apkroot_manifest_deps

	local _line _tgt _glob _idx
	cat "$_apkroot/.Manifest-apk-installed" | while read -r _line ; do
		_tgt="${_line##*[[:space:]]}"

		if fnmatch_relative_globs "$_tgt" "$@" ; then
			printf '%s\n' "$_line"

			_idx="$(grep -h -F -e "$_line" "$_apkroot/.libs.INDEX" "$_apkroot/.bins.INDEX" | cut -f 1)"
			[ "$_idx" ] && grep -h -F -e "$_idx" "$_apkroot/.libs.DEPS" "$_apkroot/.bins.DEPS" | tr -s ' \t' '\n' \
				| grep -h -F -f - "$_apkroot/.libs.INDEX" "$_apkroot/.bins.INDEX" | cut -f 2,3,4
		fi

	done | sort -k1 -k3 -k2 -u
}


# Usage: apkroot_manifest_subset_cpio <outfile|-> <globs>...
apkroot_manifest_subset_cpio() {
	local _arch="$APKARCH" && [ "$_arch" ] || ! warning "Called without value set in \$APKARCH!" || return 1
	local _apkroot="$APKROOT" && [ "$_apkroot" ] || ! warning "Called without value set in \$APKROOT!" || return 1
	local _out="$1" ; shift
	local _ret
	if [ "$_out" = "-" ] ; then
		(cd "$_apkroot" ; apkroot_manifest_subset "$@" | cut -f 3 | sort -u | cpio -H newc -o ) || ! _ret=$? || ! warning "Could not create cpio archive to stdout from subset of '$_apkroot'!" || return $_ret
	else
		touch "$_out" && file_is_writable "$_out" || ! warning "Could not create output file '$_out'!" || return 1
		(cd "$_apkroot" ; apkroot_manifest_subset "$@" | cut -f 3 | sort -u | cpio -H newc -o > "$_out" ) || ! _ret=$? || ! warning "Could not create cpio archive '$_out' from subset of '$_apkroot'!" || return $_ret
	fi
}


# Usage: apkroot_manifest_subset_cpiogz <outfile|-> <globs>...
apkroot_manifest_subset_cpiogz() {
	local _arch="$APKARCH" && [ "$_arch" ] || ! warning "Called without value set in \$APKARCH!" || return 1
	local _apkroot="$APKROOT" && [ "$_apkroot" ] || ! warning "Called without value set in \$APKROOT!" || return 1
	local _ret
	local _out="$1" ; shift
	if [ "$_out" = "-" ] ; then apkroot_manifest_subset_cpio "-" "$@" | gzip -f -q -9 || _ret=$? || ! warning "Could not compress cpio archive stream using 'gzip -9'! "
	else apkroot_manifest_subset_cpio "-" "$@" | gzip -f -q -9 > "$_out" || ! _ret=$? || ! warning "Could not compress cpio archive stream using 'gzip -9 > \"$_out\"'! " ; fi
	return $_ret
}

