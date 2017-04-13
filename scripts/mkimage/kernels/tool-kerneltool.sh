###
### KERNELTOOL
###
### Kernel components build and staging tool.

# Tool: kerneltool
# Commands: stage depmod

tool_kerneltool() { return 0 ; }
kerneltool() {
	info_prog_set "kerneltool"
	#: --staging-dir
	#: --arch
	#: --kernel-release <krel|apk|build-dir|install-dir|latest>
	#: stage [arch|arch/krel] <apk|build-dir|package-name)>  (i.e. /usr/src/linux or linux-firmware)
	#: depmod <basdir> <kernel release>
	#: subset_modules
	#: install
	#: package


	local command
	command="$1"

	case "$1" in
		stage) shift ; unset UNSTAGE RESTAGE ; kerneltool_stage "${STAGING_ROOT}" ${OPT_arch:+"$OPT_arch"} "$@" ; return $? ;;
		restage) shift ; unset UNSTAGE ; RESTAGE="yes" ; kerneltool_stage "${STAGING_ROOT}" ${OPT_arch:+"$OPT_arch"} "$@" ; return $? ;;
		unstage) shift ; unset RESTAGE ; UNSTAGE="yes" ; kerneltool_stage "${STAGING_ROOT}" ${OPT_arch:+"$OPT_arch"} "$@" ; return $? ;;
		depmod) shift ; kerneltool_depmod "${STAGING_ROOT}/${OPT_arch:-"$(_apk --print-arch)"}/merged-$1" "$1" ; return $? ;;
		--*) warning "Unhandled global option '$1'!" ; return 1 ;;
		*) warning "Unknown command '$1'!" ; return 1 ;;
	esac
}

# Command: stage
# Usage: kerneltool_stage <staging basedir> [<arch>] <kernel (apk | build dir | package name | version) > [ other (build dirs | package names) ...]
kerneltool_stage() {
	info_prog_set "kerneltool-stage"
	info_func_set "stage"
	local stage_base="$1" ; shift
	local _arch _karch _krel k_pkg k_pkgname k_pkgver
	local _arch_krel _arch_pkg_ver

	# Set our arch if it was explicitly specified, then shift if found.
	local _allarchsglob="$(get_all_archs_with_sep ' | ')"
	eval "case \"\$1\" in $_allarchsglob ) _arch=\"\$1\" ; shift ;; esac"
	
	# TODO: kerneltool_stage - Handle kernel images/tarballs.

	# Detect our arch and kernel release from the first argument.
	info_func_set "stage (detect arch/krel)"
	[ "$1" ] || set -- linux-vanilla linux-firmware
	[ "$1" = "current" ] && shift  && set -- "$(uname -r)" "$@"
	[ "$1" = "latest" ] && shift && set -- linux-vanilla "$@"
	case "$1" in
		*.apk)	# Handle explicit kernel .apk
			_arch_pkg_ver="$(apk_get_arch_package_version $1)"
			_arch_krel="$(kerneltool_apk_get_arch_kernel_release $1)"
			if ! [ "$_arch" ] ; then _arch="$_arch_krel%%/*}"
			elif [ "${_arch_krel%%/*}" = "$_arch" ] ; then warning "Kernel in apk '$1' is for arch '${_arch_krel%%/*}' but '$_arch' requested!" ; return 1 ; fi
			;;
		/*/*|./*|../*)	# Handle kernel build directory
			_arch_krel="$(kerneltool_custom_get_arch_kernel_release $1)"
			if ! [ "$_arch" ] ; then _arch="${_arch_krel%%/*}"
			elif [ "${_arch_krel%%/*}" = "$_arch" ] ; then warning "Kernel at '$1' configured for arch '${_arch_krel%%/*}' but '$_arch' requested!" ; return 1 ; fi
			;;
		*/*)  # Handle arch/krel pairs
			_arch_krel="$1"
			_arch="${_arch_krel%%/*}" && all_archs_has $_arch || ! warning "Specified arch '$_arch' not recognized!" || return 1
			_krel="${_arch_krel#*/}"
			;;
		# Handle packages with version
		linux-*-[0-9].*-r[0-9]*) k_pkg="$1" ;;
		linux-*-[0-9].*) k_pkg="$1-r0" ;;
		# Handle alpine package name with no version
		linux-*) k_pkgname="$1" ;;
		# Handle 'linux-<krel>' (with or without -r)
		linux-[0-9].*-r[0-9]*-*|linux-[0-9].*-r[0-9]*) _krel="${1#linux-}" ; _krel="${_krel/-r/-}" ;;
		linux-[0-9].*-*-*|linux-[0-9].*-*|linux-[0-9].*) _krel="${1#linux-}" ;;
		# Handle raw kernel version (uname -r)
		[0-9].*-r[0-9]*-*|[0-9].*-r[0-9]*) _krel="${1/-r/-}" ;;
		[0-9].*-*-*|[0-9].*-*|[0-9].*) _krel="$1" ;;
		*) warning "Unknown kernel spec: '$1'" ; return 1 ;;
	esac

	# If nothing has specified an arch yet, default to the system's arch.
	[ "$_arch" ] || _arch="$(_apk --print-arch)"
	
	info_func_set "stage (setup)"
	# Setup staging base directory.
	mkdir_is_writable "$stage_base" && stage_base="$(realpath $stage_base)" || ! warning "Failed to create staging base directory '$stage_base'" || return 1

	# Setup staging directory for this arch.
	mkdir_is_writable "$stage_base/$_arch" || ! warning "Failed to create writable directory for staging arch '$_arch'!" || return 1

	# Setup staging apkroot for this arch.
	local stage_apkroot="$stage_base/$_arch/apkroot"

	# Setup alpine-keys so we can actually fetch things for our desired arch. We do this BEFORE we setup our apkroot.
	if dir_not_exists "$stage_apkroot/etc/apk/keys" ; then
		mkdir_is_writable "$stage_apkroot/etc/apk/keys" || ! warning "Failed to setup staging apkroot at '$stage_apkroot'!" || return 1
		_apk fetch -s alpine-keys | tar -C "$stage_apkroot" -xz usr/share/apk/keys || ! warning "Could not fetch package 'alpine-keys' or extract '/usr/share/apk/keys' to '$stage_apkroot'!" || return 1
		cp -L "$stage_apkroot/usr/share/apk/keys/$_arch/"* "$stage_apkroot/etc/apk/keys"
	fi

	# Initilize our apk root
	apkroot_init "$_arch" "$stage_apkroot"
	apkroot_tool $OPT_apkroot_tool_cmdline
	info_func_set "stage (setup)"
	_apk update ${VERBOSE--q} || warning "Could not update apk database. Continuing..."

	# Setup apk storage dir.
	local stage_apkstore="$stage_base/$_arch/apks"
	mkdir_is_writable "$stage_apkstore" || ! warning "Failed to setup staging apkstore at '$stage_apkstore'!" || return 1
	
	# Create a manifests directory for this arch.
	local manifests="$stage_base/$_arch/manifests"
	mkdir_is_writable "$manifests" || ! warning "Failed to create directory for staging manifests '$manifests'" || return 1

	# Derive the full package atom when given a package name only.
	[ "$k_pkgname" ] && [ ! "$k_pkg" ] && k_pkg="$(apk_pkg_full "$k_pkgname")"
	# Split package atom to into package name and version.
	k_pkgname="${k_pkg%-*-*}"
	k_pkgver="${k_pkg#$k_pkgname-}"

	# Derrive the kernel release from the package (fetching if needed).
	if ! [ "$_krel" ] && [ "$k_pkgname" ] ; then
		file_is_readable "$stage_apkstore/$k_pkg.apk" || kerneltool_apk_fetch "$_arch" "$_krel" "$k_pkgname"  "$stage_apkstore" || ! warning "Could not fetch '$k_pkg'!" || return 1
		_arch_krel="$(kerneltool_apk_get_arch_kernel_release "$stage_apkstore/$k_pkg.apk")"
		[ "${_arch_krel%%/*}" != "$_arch" ] && warning "Kernel in apk '$1' is for arch '${_arch_krel%%/*}' but '$_arch' requested!" && return 1
		_krel="${_arch_krel#*/}"
	fi
	if ! [ "$_krel" ] ; then
		_krel="${k_pkgver/-r/-}-${k_pkgname#linux-}"
		warning "Could not determine kernel release for '$k_pkgname' in a reliable manner, falling back to using package version number."
	fi

	# Setup kernel staging for this arch and kernel release (restage/unstage here if requested); Set _kernpkg_ to the package name for the kernel if needed.
	local stage_kern="$stage_base/$_arch/$_krel"
	if [ "$RESTAGE" = "yes" ] || [ "$UNSTAGE" = "yes" ] ; then
		! dir_exists "$stage_kern" || rm -rf "$stage_kern" || ! warning "Could not purge staging directory at '$stage_kern'!" || return 1
		[ "$UNSTAGE" = "yes" ] && msg "Unstaged '$_arch/$_krel'." && return 0
	fi

	if dir_exists "$stage_kern" || mkdir_is_writable "$stage_kern" ; then _kernpkg_="${k_pkgname:-yes}"
	else warning "Failed to create staging kernel directory '$stage_kern'" ; return 1
	fi

	info_func_set "stage"
	# Loop over items to be staged
	local i _type _subdir
	for i do
		i="${i%/}"
		# Fix the mess if we gave it something other than a simple package name as our original arg
		[ "${_kernpkg_##yes}" ] && i="$_kernpkg_"

		# Figure out what type we're dealing with and set things up.
		case "$i" in
			*.apk)
				_type="apk"
				_subdir="${i##*/}" && _subdir="${_subdir%.apk}"
				cp -f "$i" "$stage_apkstore" && i="${i##*/}"
				;;
			*/*)	
				_type="kbuild"
				_subdir="kbuild-{i##*/}"
				;;
			*)
				_type="apk"
				local _i="$i"
				i="$(apk_pkg_full "${i%-*-*}")"
				if ! [ "$i" ] ; then warning "Could not find package matching '$_i'!" ; return 1
				elif [ "$_i" = "${i%-*-*}" ] ; then msg "Using '$i' for package '$_i'."
				elif [ "$i" != "$_i" ] ; then warning "Specified package '$_i' but found '$i'! Continuing with '$i'..." ; fi
				_subdir="apk-$i"
				
				;;
		esac


		# Handle fetching apks as needed.
		case "$_type" in
			apk)
				file_is_readable "$stage_apkstore/$i.apk" || kerneltool_apk_fetch "$_arch" "$_krel" "$i" "$stage_apkstore" || ! warning "Could not fetch '$i'!" || return 1
				i="$stage_apkstore/$i.apk"

				_apkmanifest="$manifests/${i##*/}.Manifest"
				if file_is_readable "$_apkmanifest" && verify_file_checksums "$i" "$(sed -n -e '2 p' "$_apkmanifest")" ; then
					msg "Checksum matches for '${i##*/}' in APK Manifest '$_apkmanifest'."
				elif file_is_readable "$_apkmanifest" ; then
					msg "Checksum found for '${i##*/}' in Manifest '$_apkmanifest' does not match actual checksum of verified apk in '${i}', regenerating."
					rm -f "$_apkmanifest" || ! warning "Could not remove stale '$_apkmanifest'!" || return 1
				elif file_exists "$_apkmanifest" ; then
					warning "Manifest '$_apkmanifest' exists, but is unreadable -- attempting to regenerate!"
					rm -f "$_apkmanifest" || ! warning "Could not remove bad '$_apkmanifest'!" || return 1
				fi

				if file_not_exists "$_apkmanifest" ; then 
					msg "Building APK Manifest for '$i'..."
					apk_build_apk_manifest "$manifests" "$i" || ! warning "Failed to create APK Manifest for '$i!'" || return 1
					msg2 "Done."
				fi

				;;
		esac

		# Set the list of sub-parts to be staged for each package, then iterate over it.
		kerneltool_stage_parts="${kerneltool_stage_parts:-"${_kernpkg_:+"kernel "} modules firmware dtbs headers"}"
		for p in $kerneltool_stage_parts ; do
			info_func_set "stage ($p)"
			_mymanifest="$manifests/$_subdir-$p.Manifest"
			_myddir="$stage_kern/$_subdir-$p"

			
			if dir_exists "$_myddir" ; then
				if file_exists "$_mymanifest" ; then
					info_func_set "stage ($p check-manifest)"
					msg "Directory '$_subdir-$p' exists in '$stage_kern', checking contents against manifest '$_mymanifest'..."
					kerneltool_verify_path_manifest "$_myddir" "$_mymanifest" && msg "Matched." && continue
					warning "Contents of directory '$_myddir' does not match manifest '$_mymanifest'!" 
				fi
				info_func_set "stage ($p)"
				msg "Rebuilding directory '$_subdir-$p' in '$stage_kern'."
				rm -rf "$_myddir" || ! warning "Could wipe destination directory for '$_subdir-$p' in '$stage_kern'!" || return 1
			fi
			info_func_set "stage ($p)"
			mkdir_is_writable "$_myddir" || ! warning "Could not create writable destination directory for '$_subdir-$p' in '$stage_kern'!" || return 1

			# Extract / install this subpart.
			case "$_type" in
				apk) 
					info_func_set "stage ($p-extract)"
					msg "Extracting '$i' to '$_subdir-$p' in '$stage_kern'."
					kerneltool_apk_extract_$p "$_arch" "$_krel" "$i" "$stage_kern/$_subdir-$p" || ! warning "Failed to extract $p from '$i' into '$_myddir'!" || return 1
					;;
				kbuild)
					info_func_set "stage ($p-install)"
					msg "Installing $p from '$i' into staging dir '$_myddir'."
					kerneltool_kbuild_make_${p}_install "$_arch" "$_krel" "$i" "$stage_kern/$_subdir-$p" || ! warning "Failed to stage $p from '$i' into '$_myddir'!" || return 1
					;;
			esac

			# Postprocess this subpart.
			case "$p" in
				kernel)
					if dir_exists "$_myddir/boot" ; then
						info_func_set "stage ($p-post)"
						( cd "$_myddir/boot" ; for _f in * ; do if file_exists "$_f" ; then mv "$_f" "${_f}-$_krel" && ln -s "${_f}-$_krel" "$_f" ; fi ; done) ;
					fi
				;;

				modules)
					if dir_exists "$_myddir/lib/modules" ; then
						info_func_set "stage ($p-post check-versions)"
						# Check that the vermagic version of the module matches the requested kernel release
						local _vermismatches="$( cd "$_myddir" \
							&& find -type f \( -iname '*.ko' -o -iname '*.ko.*' \) -exec sh -c "modinfo -F vermagic {} | cut -d' ' -f 1 | grep -q -v '^$_krel'" \; -print \
							| sed 's/^[[:space:]]*$//g' )"
						[ "$_vermismatches" ] && warning "Mismatch between kernel release '$_krel' and vermagic value in modules:" && warning2 "$_vermismatches" && return 1

						# Build kmod index.
						info_func_set "stage ($p-post build-index)"
						local _mymod _mymodname
						local _mymodindex="$manifests/${_myddir##*/}.kmod-INDEX"
						: > "$_mymodindex" && file_is_writable "$_mymodindex" || ! warning "Could not write to module index at '$_mymodindex'!" || return 1

						(	cd "$_myddir"
							printf '# Module index for: %s/%s-%s\n' "$_arch" "$_subdir" "$p"
							printf '# Kernel: %s/%s\n' "$_arch" "$kver"

							find "lib/modules" -type f \( -iname '*.ko' -o -iname '*.ko.*' \) -print | while read _mymod; do
								_mymodname="${_mymod##*/}" && _mymodname="${_mymodname%.ko.*}" && _mymodname="${_mymodname%.ko}"
								printf 'kmod:%s:%s/%s:%s\t' "$_mymodname" "$_arch" "$_krel" "$(kerneltool_checksum_module "$_myddir/$_mymod")"
								printf 'kpkg:%s/%s-%s\t' "$_arch" "$_subdir" "$p"
								calc_file_checksums "$_mymod" sha512 ; printf '\n'
							done
						) >> "$_mymodindex" || ! warning "Failed to create module index '$_mymodindex' for '$_myddir'!" || return 1
						unset _mymod _mymodname _mymodindex
					fi
				;;

				firmware)
					if dir_exists "$_myddir/lib/firmware" ; then
						# Build firmware index.
						info_func_set "stage ($p-post build-index)"
						local _myfw
						local _myfwindex="$manifests/${_myddir##*/}.firmware-INDEX"
						: > "$_myfwindex" && file_is_writable "$_myfwindex" || ! warning "Could not write to module index at '$_myfwindex'!" || return 1
						( 	cd "$_myddir"
							printf '# Firmware index for: %s/%s-%s\n' "$_arch" "$_subdir" "$p"
							find "lib/firmware" -type f -print | while read _myfw; do
								_myfw="${_myfw#./}"
								printf 'firmware:%s:%s:%s\t' "${_myfw#lib/firmware}" "$_arch" "$(kerneltool_checksum_module "$_myfw")"
								printf 'kpkg:%s/%s-%s\t' "$_arch" "$_subdir" "$p"
								calc_file_checksums "$_myfw" sha512 ; printf '\n'
							done
						) >> "$_myfwindex" || ! warning "Failed to create firmware index '$_mymodindex' for '$_myddir'!" || return 1
					fi
				;;
			esac

			# Generate Manifest for this subpart.
			info_func_set "stage ($p-manifest)"
			if dir_exists "$_myddir" ; then
				msg "Building Manifest for '$_myddir' at '$_mymanifest'."
				kerneltool_calc_path_manifest "$_myddir" "kpkg:$_arch/$_subdir-$p" > "$_mymanifest"
			fi
		done

		# Don't try to extract multiple kernels.
		kerneltool_stage_parts="${kerneltool_stage_parts/kernel /}"	
		unset _kernpkg_

	done

	# Wipe and create merged-root for this arch and kernel release
	info_func_set "stage (merge)"
	msg "Merging contents of all staging subdirectiories under '$_arch/$_krel' into '$_arch/merged-$_krel'."
	local merged="$stage_base/$_arch/merged-$_krel"
	[ -e "$merged" ] && rm -rf "$merged" # We want to recreate this every time even if we're only adding staged files.
	mkdir_is_writable "$merged" || ! warning "Failed to create directory for staging merged root '$merged'" || return 1

	# Create hardlinked copy of all contents of all subdirectories in stage_base into merged.
	(cd "$stage_kern" && cp -rl */* "$merged")

	# Update module dependencies.
	info_func_set "stage (depmod)"
	msg "Running depmod against staged modules for '$_arch/$_krel'."
	kerneltool_depmod "$merged" "$_krel" || ! warning "depmod for '$_krel' failed in merged dir '$merged'!" || return 1

	# Create checksummed module and firmware deps for each module.
	info_func_set "stage (kmod-deps)"
	msg "Calculating checksummed module and firmware dependencies for all modules for '$_arch/$_krel'."
	_moduledepfile="$manifests/$_krel.kmod_fw-DEPS"
	( cd "$merged"
		: > "$_moduledepfile" && file_is_writable "$_moduledepfile" || ! warning "Could not write to module and firmware dependency dump at '$_moduledepfile'!" || return 1
		find "lib/modules" -type f \( -iname '*.ko' -o -iname '*.ko.*' \) -print | while read _mymod; do
			kerneltool_calc_module_fw_deps "$_arch" "$_krel" "$merged" "$_mymod"
		done
	) >> "$_moduledepfile" || ! warning "Failed to create checkesummed kernel module and firmware dependency dump!" || return 1

	info_prog_set "kerneltool-stage"
	msg "Staging complete for '$_arch/$_krel'."

	info_func_set "stage"
	msg "Done."
	return 0
}


# Command: depmod
# Usage: kerneltool_depmod <base dir> [<kernel release>]
kerneltool_depmod() {
	info_prog_set "kerneltool-depmod"
	local _bdir="$1" _krel="$2"
	shift 2
	depmod -a -e ${VERBOSE+-w }-b "$_bdir" -F "$_bdir/boot/System.map-$_krel" $_krel 
	#2>&1 > /dev/null | grep -e '^.+$' >&2 && warning "Errors encounterd by depmod in '$_bdir'!" && return 1
}


# Command: mkmodloop
# Build our modloop
# TODO: kerneltool -  Modify build_kernel_stage_modloop to allow selecting which modules are installed in generated modloop!
kerneltool_mkmodloop() {
	info_prog_set "kerneltool-mkmodloop"

	local _outname="modloop-$_flavor"
	#kerneltool_build_modules_subset "$_bdir" "$_tmp"

	mkdir "$_tmp"
	mksquashfs "$_tmp" "$_out/boot/$_outname" -comp xz -exit-on-error
}


# Print kernel arch given system arch.
# Usage: get_karch_from_arch <arch>
kerneltool_get_karch_from_arch() {
	local _karch="$(getvar arch_$1_kernel_arch_name)"
	[ "$_karch" ] && printf '%s' "$_karch" && return 0
	warning "No \$arch_$1_kernel_arch_name set!" ; return 1
}


# Find system arch for given kernel arch.
# Usage: get_arch_from_karch <kernel arch>
kerneltool_get_arch_from_karch() {
	local a
	for a in $(get_all_archs) ; do
		[ "$(kerneltool_get_karch_from_arch "$a")" = "$1" ] && printf '%s' "$a" && return 0
	done
	return 1
}


# TODO: kerneltool - Finish build_modules_subset to allow filtering of modules for building modloop/mkinitfs/etc.
kerneltool_build_modules_subset() {
	local _bdir="$1"
	local _ddir="$2"
	shift 2
	_modbase="$_bdir/lib/modules"
	_modout="$_ddir/lib/modules"
	_fwbase="$_bdir/lib/firmware"
	_fwout="$_ddir/lib/firmware"

	dir_is_readable "$_modbase" || ! warning "Could not read merged modules directory '$_modbase'" || return 1
	mkdir_is_writable "$_modout" || ! warning "Could not create output directory for modules subset at '$_modout'!" || return 1
	

	# NOTE: Allow filtering of modloop modules here!

	for _mod do
		case "$_mod" in 
		*/*) ;;
		*) ;;
		esac
	done

	dir_is_readable "$_fwbase" && ! mkdir_is_writable "$_fwout" && warning "Could not create output directory for firmware at '$_fwout'!" && return 1

	find "$_modout" -type f \( -iname '*.ko' -o -iname '*.ko.*' \) -exec printf '%s\t' \{\} \; -exec modinfo -F firmware \{\} \; | while read _mod _fw; do
		file_is_readable "$_fwout/$_fw" \
			|| file_is_readable "$_fwbase/$_fw" && install -pD "$_fwbase/$_fw" "$_fwout/$_fw" \
			|| ! warning "Could not find firmware '$_fw' in '$_fwbase' needed by module '$_mod'!" || return 1
	done
}


# Calculate complete module dependency tree (including firmware) for the given list of modules, with checksums.
# Usage: kerneltool_calc_module_fw_deps <arch> <kernel release> <base dir> <modules...>
kerneltool_calc_module_fw_deps() {
	local _arch="$1" _krel="$2" _bdir="$3"
	shift 3
	local _fwbase="$_bdir/lib/firmware"
	local _mod _myfile _mydeps _mydepfiles _vermagic _myfw _fw _mc _sums _file _mymod
	for _mymod in $@ ; do
		_sums="" _mydeps="" _mydepfiles="" _fw=""
		_myfile="$(modinfo -b "$_bdir" -k "$_krel" -F filename "$_mymod")"
		_mymod="${_myfile##*/}" && _mymod="${_mymod%%.ko*}"
		[ "$VERBOSE" ] && msg "Calculating module and firmware dependencies for '$_mymod'."

		# Get list of deps for mod and replace ',' with newline.
		_mydeps="$(modinfo -b "$_bdir" -k "$_krel" -F depends "$_myfile" | sed 's/,/\n/g' )"

		# Get filenames for each dep.
		for _mod in $_mydeps ; do _mydepfiles="${_mydepfiles+"$_mydepfiles "}$(printf '%s' $(modinfo -b "$_bdir" -k "$_krel" -F filename "$_mod"))"; done

		# Do some sanity checks, then get checksums for mod and deps (tab sep)
		for _file in $_myfile $_mydepfiles ; do
			# Parse module out of filename and make sure we can read file
			_mod="${_file##*/}" && _mod="${_mod%.ko*}"
			file_is_readable "$_file" || ! warning "Could not read file '$_file' for module '$_mod'!" || return 1

			# Check that the vermagic version of the module matches the requested kernel release
			_vermagic="$(modinfo -b "$_bdir" -k "$_krel" -F vermagic "$_file" | cut -d' ' -f 1)"
			[ "$_vermagic" = "$_krel" ] || ! warning "Mismatch between kernel release '$_krel' and module '$_file' vermagic value of '$_vermagic'!" || return 1
			[ "$VERBOSE" ] && msg "Module '$_mymod' depends on '$_mod'."

			# Determine which tool to use to cat compressed modules for checksumming
			case "$_file" in *.ko)_mc='cat';; *.ko.bz*)_mc='bzcat';; *.ko.gz)_mc='zcat';; *.ko.lz4)_mc='lz4cat';; *.ko.lzo*)_mc='lzopcat';; *.ko.xz|*.ko.lz*)_mc='xzcat';; esac
			# Checksum this module and add it to the end of the list
			_sums="${_sums}$(printf 'kmod:%s:%s/%s:%s\t' "$_mod" "$_arch" "$_krel" "$(kerneltool_checksum_module "$_file")" )"

			# Find list of required firmware for each module, if any.
			_fw="$(modinfo -b "$_bdir" -k "$_krel" -F firmware "$_file" )"
			for _myfw in $_fw ; do
				_file="$_fwbase/$_myfw"
				if file_is_readable "$_file" ; then 
					_sums="${_sums}$(printf 'firmware:%s:%s:%s\t' "$_myfw" "$_arch" "$(kerneltool_checksum_module "$_file")" )"
					[ "$VERBOSE" ] && msg "Module '$_mod' depends on '$_myfw'."
				else 
					_sums="${_sums}$(printf 'firmware:%s:%s:%s\t' "$_myfw" "$_arch" "UNRESOLVED" )"
					[ "$VERBOSE" ] && msg "Could not find firmware '$_myfw' in '$_fwbase' needed by module '$_mod'!"
				fi
			done
		done
		printf '%s\n' "$_sums"
	done
}


# Get sha512 checksum of uncompressed module given a compressed or uncompressed module, or checksum of raw file (firmware) with no decompression applied.
# Usage: kerneltool_checksum_module <kernel module file>
kerneltool_checksum_module() {
	local _file="$1" _mc
	file_is_readable "$1" || ! warning "Could not read module '$1'!" || return 1
	case "$_file" in
		*.ko) : uncompressed module; _mc='cat';;
		*.ko.bz*) : compressed module; _mc='bzcat';; *.ko.gz)_mc='zcat';; *.ko.lz4)_mc='lz4cat';; *.ko.lzo*)_mc='lzopcat';; *.ko.xz|*.ko.lz*)_mc='xzcat';;
		*) : unrecognized module extension, checksum raw file ; _mc='cat'
	esac
	$_mc "$_file" | sha512sum | cut -d' ' -f 1 | sed 's/^/sha512:/g'
}

