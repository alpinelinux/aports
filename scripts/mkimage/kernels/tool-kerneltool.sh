####
###  KERNELTOOL
###
###  Kernel components build and staging tool.
####

# Tool: kerneltool
# Commands: help stage restage unstage depmod mkmodsubset mkmodcpio mkmodcpiogz mkmodloop

# Usage: tool_kerneltool
tool_kerneltool() { return 0 ; }

# Usage: kerneltool
kerneltool() {
	info_prog_set "kerneltool"

	local command
	command="$1"

	: "${OPT_apkroot_setup_cmdline:="--repositories-file /etc/apk/repositories --host-keys"}"
	case "$1" in
		list) shift ; kerneltool_list_staged ; return $? ;;
		init) shift ; kerneltool_init "$@" ; return $? ;;
		stage) shift ; unset UNSTAGE RESTAGE ; kerneltool_stage "$@" ; return $? ;;
		restage) shift ; unset UNSTAGE ; RESTAGE="yes" ; kerneltool_stage "$@" ; return $? ;;
		unstage) shift ; unset RESTAGE ; UNSTAGE="yes" ; kerneltool_stage "$@" ; return $? ;;
		depmod) shift ; kerneltool_depmod "${STAGING_ROOT}/${OPT_arch:-"$(_apk --print-arch)"}/merged-$1" "$1" $2 ; return $? ;;
		mkmodsubset) shift ; kerneltool_mkmodsubset "${STAGING_ROOT}" "${OPT_arch:-"$(_apk --print-arch)"}" "$@" ; return $? ;;
		mkmodcpio) shift ; kerneltool_mkmodcpio "${STAGING_ROOT}" "${OPT_arch:-"$(_apk --print-arch)"}" "$@" ; return $? ;;
		mkmodcpiogz) shift ; kerneltool_mkmodcpiogz "${STAGING_ROOT}" "${OPT_arch:-"$(_apk --print-arch)"}" "$@" ; return $? ;;
		mkmodloop) shift ; kerneltool_mkmodloop "${STAGING_ROOT}" "${OPT_arch:-"$(_apk --print-arch)"}" "$@" ; return $? ;;
		help) kerneltool_usage && return 0 ;;
		--*) warning "Unhandled global option '$1'!" ; return 1 ;;
		*) warning "Unknown command '$1'!" ; return 1 ;;
	esac
	return 1
}



# Print usage
# Usage: kerneltool_usage
kerneltool_usage() {
	kerneltool_commands_usage
	multitool_opts_usage
	apkroot_opts_usage
}


# Usage: kerneltool_commands_usage
kerneltool_commands_usage() {
cat <<EOF
Usage: kerneltool <global opts> <command> <command opts>

Commands:
	stage <kernel spec> [<additional apk-file/kbuild-dir/apk-atom>...]
		Stage kernel components and build manifests for use by other kernel tools.
		The <kernel spec> may be: an apk file, kernel build directory, apk package atom;
		or, an already staged kernel release or <arch>/<kernel release> pair; or, the
		symbolic names 'current', 'latest' or 'latest-<flavor>'. Package atoms are tested
		with added kernel flavor first, but an exact version match on the unflavored atom
		will take precedence over an unversioned or mismatched version on a flavored atom.

	restage <kernel spec> [<additional apk-files/kbuild-dirs/apk-atoms>...]
		Wipe staging directory and restage from scratch.

	unstage <kernel spec>
		Wipe staging directory for specified kernel spec.


	depmod <kernel release> [<subset base>]
		Run depmod against the staging directory for specified kernel release.


	mkmodsubset <kernel release> <modsubset name> [<module names/globs>...]
		Build a subset of modules for the given release containing the specified
		modules and all their deps, including firmware.

	mkmodcpio[.gz] <kernel release> [modsubset=<modsubset name>] [<module names/globs>...]
		Build a [compressed] cpio archive containg the specified subset of modules.
		An existing named subset may be used or a new one created using modsubset=<name>,
		otherwise a temporary subset will be created, used, and purged.

	mkmodloop <kernel release> [modsubset=<modsubset name>] [<module names/globs>...]
		Build a compressed squashfs filesystem for use as a modloop containing either
		the specified modules and their deps or all staged modules with needed firmware.
		Usage is the same as mkmodcpio.


EOF
}



###
##  Kerneltool Commands
###

kerneltool_init_vars() { KTYPE="$1" ; KARCH="$2" ; KREL="$3" ; KSRC="$4" ; KPATH="$5" ; [ "$KTYPE" = "kapk" ] && KFLAVOR="${KSRC#linux-}" && KFLAVOR="${KFLAVOR%-*-r*}" ; }
kerneltool_info_kstage_string() { printf '%s:%s/%s/%s:%s\n' "$KTYPE" "$KARCH" "$KREL" "$KSRC" "$KPATH" ; }
kerneltool_info_kstage_parse() {
	KTYPE="${1%%:*}"
	KARCH="${1#$KTYPE:}" && KARCH="${KARCH%%/*}"
	KREL="${1#$KTYPE:$KARCH/}" && KREL="${KREL%%/*}"
	KSRC="${1#$KTYPE:$KARCH/$KREL/}" && KSRC="${KSRC%%:*}"
	KPATH="${1%%*:}"

	[ "$KTYPE" = "kapk" ] && KFLAVOR="${KSRC#linux-}" && KFLAVOR="${KFLAVOR%-*-r*}"
}


kerneltool_apkroot_init() {
	[ "$STAGING_ROOT" ] && [ "$KARCH" ] || ! warning "kerneltool_apkroot_init needs \$STAGING_ROOT and \$KARCH defined!" || return 1
	STAGE_ARCH="$STAGING_ROOT/$KARCH"
	KAPKROOT="$STAGE_ARCH/apkroot"
	KAPKSTORE="$STAGE_ARCH/apks"
	apkroottool init "$KAPKROOT" "$KARCH"
	apkroottool setup "$KAPKROOT" "$KARCH"
	apkroottool "$KAPKROOT" apk update -q
	mkdir_is_writable "$KAPKSTORE"
}


kerneltool_init_by_apkpackage() {
	KARCH="$1"
	shift
	kerneltool_apkroot_init
	local _pkg="$(apk_pkg_full "$1")"
	apk_fetch "$KAPKSTORE" "$1"
	kerneltool_init_by_apkfile "$KARCH" "$KAPKSTORE/$_pkg.apk"
}


kerneltool_init_by_apkfile() {
	local _arch="$1" _source="$2"
	local _arch_pkg_ver="$(apk_get_apk_arch_package_version "$_source")"
	local _arch_krel="$(kerneltool_apk_get_arch_kernel_release "$_source")"
	local _pkg="${_arch_pkg_ver#*/}"
	kerneltool_init_vars "kapk" "${_arch_krel%%/*}" "${_arch_krel#*/}" "$_pkg" "$_source"
	kerneltool_apkroot_init
	file_exists "$KAPKSTORE/${KPATH##*/}" && [ "$(cat "$KPATH" | sha512sum )" = "$(cat "$KAPKSTORE/${KPATH##*/}" | sha512sum)" ] || cp -f "$KPATH" "$KAPKSTORE" || ! warning "Could not copy '$KPATH' to '$KAPKSTORE'!" || return 1
	KPATH="${KPATH##*/}"
}


kerneltool_init_by_krel() {
	local _arch="$1" _krel="$2"
	local _stage_kern="$STAGING_ROOT/$_arch/kernel/$_krel"
	dir_exists "$_stage_kern" || ! warning "No kernel staged for kernel release '$_krel' on arch '$_arch'!" || return 1
	dir_is_writable "$_stage_kern" || ! warning "Can not write to staging subdirectory '$_stage_kern'!" || return 1
	file_is_readable "$_stage_kern/.info_kstage" && kerneltool_info_kstage_parse "$(cat "$_stage_kern/.info_kstage")" || ! warning "Staging info '$_stage_kern/.info_kstage' can not be read!" || return 1
	kerneltool_apkroot_init
	return 0
}


kerneltool_init_by_path() {
	if ! [ -e "$1" ] ; then
		warning "'$1' does not exist!" ; return 1
	elif [ -d "$1" ] ; then dir_is_readable "$1" || ! warning "Can not read specified directory '$1'!" || return 1

		# Handle the case where we're passed a subdirectory under our staging directory.
		case "$1" in "$stage_base"/*/*/*)
			local _arch="${1#$stage_base/}" ; local _krel="${_arch#*/*/}" ; _krel="${_krel%%/*}" ; _arch="${_arch%%/*}"
			kerneltool_init_by_krel "$_arch" "$_krel" ; return $?
			;;
		esac

		# Handle the case where we're passed a directory containing Kbuild system.
		file_exists "$1/Kbuild" && kerneltool_init_kernel_by_kbuild "$1" ; return $?

	elif [ -f "$1" ] ; then file_is_readable "$1" || ! warning "Can not read specified file '$1'!" || return 1
		case "$1" in 
			*.apk) kerneltool_init_by_apkfile "$1" ; return $? ;;
			*) warning "Don't know how to handle file '$1'!" ; return 1 ;;
		esac

	else warning "Path '$1' is not a file or directory!" ; return 1 ; fi
}


kerneltool_init_by_kbuild() {
	local _arch="$1" _source="${2%/}"

	local _arch_krel="$(kerneltool_custom_get_arch_kernel_release "$_source")"
	[ "$_arch_krel" ] || ! warning "Could not detect architecture and kernel release for '$_source'!" || return 1
	! [ "$_arch" ] || [ "$_arch" = "${_arch_krel%%/*}" ] || ! warning "Requested arch '$_arch' but kernel is configured for '${_arch_krel%%/*}' in '$_source'!" || return 1

	kerneltool_init_vars "kbuild" "${_arch_krel%%/*}" "${_arch_krel#*/}" "${_source##*/}" "${_source}"
	kerneltool_apkroot_init
}

# Must be called with STAGING_ROOT defined
kerneltool_init() {
	[ "$STAGING_ROOT" ] || ! warning "\$STAGING_ROOT must be defined before calling kerneltool_init!" || return 1
	local _current_krel="$(uname -r)"
	local _host_arch="$(_apk --print-arch)"

	case "$1" in

	# keywords
	current ) kerneltool_init_by_krel "$_host_arch" "$_current_krel" ;;
	latest ) kerneltool_init_by_apkpackage "${OPT_arch:-$_host_arch}" "linux-${_current_krel##*-}" ;;
	latest-*[!/] ) kerneltool_init_by_apkpackage "${OPT_arch:-$_host_arch}" "linux${1#latest}" ;;
	custom ) kerneltool_init_by_path "${OPT_arch:-$_host_arch}" "/usr/src/linux" ;;

	# path
	/* | ./* | ../* | *.apk ) kerneltool_init_by_path "${OPT_arch}" "$1" ;;

	# apk package
	linux-* ) kerneltool_init_by_apkpackage "${OPT_arch:-$_host_arch}" "$1" ;;

	# Kernel release string
	[0-9]*.[0-9]* ) kerneltool_init_by_krel "${OPT_arch:-$_host_arch}" "$1" ;;

	# Something else...
	*) return 1 ;;

	esac

	STAGE_ARCH="$STAGING_ROOT/$KARCH"
	STAGE_KERN="$STAGE_ARCH/kernel"

	STAGE_KREL="$STAGE_KERN/$KREL"
	mkdir_is_writable "$STAGE_KREL" || ! warning "Could not create staging subdir '$STAGE_KREL'!" || return 1
	kerneltool_info_kstage_string > "$STAGE_KREL/.info_kstage"

	STAGE_MANIFESTS="$STAGE_ARCH/Manifests"
	mkdir_is_writable "$STAGE_MANIFESTS" || ! warning "Could not create staging Manifest directory '$STAGE_MANIFESTS'!" || return 1
}


kerneltool_list_staged() {
	dir_exists "$STAGING_ROOT" || ! warning "No staging directory at '$STAGING_ROOT'!" || return 1
	( cd "$STAGING_ROOT" && for a in * ; do dir_exists "$a/kernel" && cd "$STAGING_ROOT/$a/kernel" && for b in * ; do file_exists "$b/.info_kstage" && cat "$STAGING_ROOT/$a/kernel/$b/.info_kstage" ; done ; done )

}

# Command: stage
# Usage: kerneltool_stage <staging basedir> [<arch>] <kernel (apk | build dir | package name | version) > [ other (build dirs | package names) ...]
kerneltool_stage() {
	info_prog_set "kerneltool-stage"
	info_func_set "stage"

	kerneltool_init "$1" ; shift
	if [ "$KTYPE" = "kbuild" ] ; then _k="$KPATH"
	elif [ "$KTYPE" = "kapk" ] ; then _k="$KAPKSTORE/$KPATH"
	else warning "Don't know to stage kernel with source type '$KTYPE'!" ; return 1 ; fi

	# Setup kernel staging for this arch and kernel release (restage/unstage here if requested); Set _kernpkg_ to the package name for the kernel if needed.
	if [ "$RESTAGE" = "yes" ] || [ "$UNSTAGE" = "yes" ] ; then
		! dir_exists "$STAGE_KERN" || rm -rf "$STAGE_KERN" || ! warning "Could not purge staging directory at '$STAGE_KERN'!" || return 1
		[ "$UNSTAGE" = "yes" ] && msg "Unstaged '$KARCH/$KREL'." && return 0
	fi

	if dir_exists "$STAGE_KERN" || mkdir_is_writable "$STAGE_KERN" ; then :
	else warning "Failed to create staging kernel directory '$STAGE_KERN'" ; return 1
	fi

	info_func_set "stage"

	# Loop over items to be staged
	local i _type _subdir

	for i in "$_k" "$@" ; do
		i="${i%/}"

		# Figure out what type we're dealing with and set things up.
		case "$i" in
			*.apk)
				_type="kapk"
				_subdir="${i##*/}" && _subdir="${_subdir%.apk}"
				file_exists "$KAPKSTORE/${i##*/}" && [ "$(cat "$i" | sha512sum )" = "$(cat "$KAPKSTORE/${i##*/}" | sha512sum)" ] || cp -f "$i" "$KAPKSTORE" || ! warning "Could not copy '$i' to '$KAPKSTORE'!" || return 1
				i="${i##*/}"
				;;
			/* | ./* | ../* )
				_type="kbuild"
				_subdir="kbuild-${i##*/}"
				;;
			*)
				_type="kapk"

				local _i="$i" _in="${i%-*-r*}"
				local _iv="${i#$_in}"
				local _if="${_in%-$KFLAVOR}-$KFLAVOR$_iv"
				
				i="$(apk_pkg_full "$i" )"
				f="$(apk_pkg_full "$_if")"
				[ "$i" ] || [ "$f" ] || ! warning "Could not find package matching '$_i'!" || return 1
				if [ "$_if" = "$f" ] ; then i="$f" ; msg "Using package '$f'."
				elif [ "$_i" = "$i" ] ; then msg "Using package '$i'."
				elif [ "$_if" = "${f%-*-*}" ] ; then i="$f" ; msg "Using package '$f' for '$_i'."
				elif [ "$_i" = "${i%-*-*}" ] ; then msg "Using package '$i' for '$_i'."
				elif [ "${_if%-*-*}" = "${_i%-*-*}" ] ; then i="$f" ; warning "Specified package '$_i' but found '$i'! Continuing with '$i'..."
				elif [ "${_if%-*-*}" = "${_i%-*-*}" ] ; then warning "Specified package '$_i' but found '$i'! Continuing with '$i'..." ; fi
				_subdir="apk-$i"
				;;
		esac


		# Handle fetching apks as needed.
		case "$_type" in
			kapk)
				apk_fetch "$KAPKSTORE" "$i" || ! warning "Could not fetch '$i'!" || return 1
				i="$KAPKSTORE/${i%.apk}.apk"

				_apkmanifest="$STAGE_MANIFESTS/${i##*/}.Manifest"
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
					apk_build_apk_manifest "$STAGE_MANIFESTS" "$i" || ! warning "Failed to create APK Manifest for '$i!'" || return 1
					msg2 "Done."
				fi

				;;
		esac

		# Set the list of sub-parts to be staged for each package, then iterate over it.
		kerneltool_stage_parts="kernel modules firmware dtbs headers"
		for p in $kerneltool_stage_parts ; do
			info_func_set "stage ($p)"
			_mymanifest="$STAGE_MANIFESTS/$_subdir-$p.Manifest"
			_myddir="$STAGE_KREL/$_subdir-$p"

			
			if dir_exists "$_myddir" ; then
				if file_exists "$_mymanifest" ; then
					info_func_set "stage ($p check-manifest)"
					msg "Directory '$_subdir-$p' exists in '$STAGE_KREL', checking contents against manifest '$_mymanifest'..."
					kerneltool_verify_path_manifest "$_myddir" "$_mymanifest" && msg "Matched." && continue
					warning "Contents of directory '$_myddir' does not match manifest '$_mymanifest'!" 
				fi
				info_func_set "stage ($p)"
				msg "Rebuilding directory '$_subdir-$p' in '$STAGE_KREL'."
				rm -rf "$_myddir" || ! warning "Could wipe destination directory for '$_subdir-$p' in '$STAGE_KREL'!" || return 1
			fi
			info_func_set "stage ($p)"
			mkdir_is_writable "$_myddir" || ! warning "Could not create writable destination directory for '$_subdir-$p' in '$STAGE_KREL'!" || return 1

			# Extract / install this subpart.
			case "$_type" in
				kapk) 
					info_func_set "stage ($p-extract)"
					msg "Extracting '$i' to '$_subdir-$p' in '$STAGE_KREL'."
					kerneltool_apk_extract_$p "$KARCH" "$KREL" "$i" "$STAGE_KREL/$_subdir-$p" || ! warning "Failed to extract $p from '$i' into '$_myddir'!" || return 1
					;;
				kbuild)
					info_func_set "stage ($p-install)"
					msg "Installing $p from '$i' into staging dir '$_myddir'."
					kerneltool_kbuild_make_${p}_install "$KARCH" "$KREL" "$i" "$STAGE_KREL/$_subdir-$p" || ! warning "Failed to stage $p from '$i' into '$_myddir'!" || return 1
					;;
			esac

			# Postprocess this subpart.
			case "$p" in
				kernel)
					if dir_exists "$_myddir/boot" ; then
						info_func_set "stage ($p-post)"
						( cd "$_myddir/boot" ; for _f in * ; do if file_exists "$_f" ; then mv "$_f" "${_f%-$KFLAVOR}-$KREL" && ln -s "${_f%-$KFLAVOR}-$KREL" "$_f" ; fi ; done) ;
					fi
				;;

				modules)
					if dir_exists "$_myddir/lib/modules" ; then
						info_func_set "stage ($p-post check-versions)"
						# Check that the vermagic version of the module matches the requested kernel release
						local _vermismatches="$( cd "$_myddir" \
							&& find -type f \( -iname '*.ko' -o -iname '*.ko.*' \) -exec sh -c "modinfo -F vermagic {} | cut -d' ' -f 1 | grep -q -v '^$KREL'" \; -print \
							| sed 's/^[[:space:]]*$//g' )"
						[ "$_vermismatches" ] && warning "Mismatch between kernel release '$KREL' and vermagic value in modules:" && warning2 "$_vermismatches" && return 1

						# Build kmod index.
						info_func_set "stage ($p-post build-index)"
						local _mymod _mymodname
						local _mymodindex="$STAGE_MANIFESTS/${_myddir##*/}.kmod-INDEX"
						: > "$_mymodindex"
						file_is_writable "$_mymodindex" || ! warning "Could not write to module index at '$_mymodindex'!" || return 1

						(	cd "$_myddir"
							printf '# Module index for: %s/%s-%s\n' "$KARCH" "$_subdir" "$p"
							printf '# Kernel: %s/%s\n' "$KARCH" "$KREL"

							find "lib/modules" -type f \( -iname '*.ko' -o -iname '*.ko.*' \) -print | while read _mymod; do
								_mymodname="${_mymod##*/}" && _mymodname="${_mymodname%.ko.*}" && _mymodname="${_mymodname%.ko}"
								printf 'kmod:%s:%s/%s:%s\t' "$_mymodname" "$KARCH" "$KREL" "$(kerneltool_checksum_module "$_myddir/$_mymod")"
								printf 'kpkg:%s/%s-%s\t' "$KARCH" "$_subdir" "$p"
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
						local _myfwindex="$STAGE_MANIFESTS/${_myddir##*/}.firmware-INDEX"
						: > "$_myfwindex"
						file_is_writable "$_myfwindex" || ! warning "Could not write to module index at '$_myfwindex'!" || return 1
						( 	cd "$_myddir"
							printf '# Firmware index for: %s/%s-%s\n' "$KARCH" "$_subdir" "$p"
							find "lib/firmware" -type f -print | while read _myfw; do
								_myfw="${_myfw#./}"
								printf 'firmware:%s:%s:%s\t' "${_myfw#lib/firmware/}" "$KARCH" "$(kerneltool_checksum_module "$_myfw")"
								printf 'kpkg:%s/%s-%s\t' "$KARCH" "$_subdir" "$p"
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
				kerneltool_calc_path_manifest "$_myddir" "kpkg:$KARCH/$_subdir-$p" > "$_mymanifest"
			fi
		done

		# Don't try to extract multiple kernels.
		kerneltool_stage_parts="${kerneltool_stage_parts/kernel /}"	
		unset _kernpkg_

	done

	kerneltool_merge
	kerneltool_depall
	
	info_prog_set "kerneltool-stage"
	msg "Staging complete for '$KARCH/$KREL'."

	info_func_set "stage"
	msg "Done."
	return 0
}

kerneltool_merge() {
	# Wipe and create merged-root for this arch and kernel release
	info_func_set "stage (merge)"
	msg "Merging contents of all staging subdirectiories under '$STAGE_KREL' into '$STAGE_KERN/merged-$KREL'."
	local merged="$STAGE_KERN/merged-$KREL"
	dir_not_exists "$merged" || rm -rf "$merged" || ! warning "Failed to wipe target directory before merging!" || return 1 # We want to recreate this every time even if we're only adding staged files.
	mkdir_is_writable "$merged" || ! warning "Failed to create directory for staging merged root '$merged'" || return 1

	# Create hardlinked copy of all contents of all subdirectories in stage_base into merged.
	(cd "$STAGE_KREL" && cp -rl */* "$merged") || ! warning "Could not merge '$STAGE_KREL/*' into '$merged'!" || return 1

	merged_manifest="$STAGE_MANIFESTS/$KREL.Manifest" merged_kmod_index="$STAGE_MANIFESTS/$KREL.kmod-INDEX" merged_firmware_index="$STAGE_MANIFESTS/$KREL.firmware-INDEX"
	# Creat merged Manifest, kmod-INDEX, and firmware-INDEX
	printf '# Manifest for %s/%s\n' "$KARCH"  "$KREL" > "$merged_manifest" && file_is_writable "$merged_manifest" || ! warning "Could not write to merged Manifest at '$merged_manifest'!" || return 1
	(cd "$STAGE_KREL" && for _d in * ; do file_exists "$manifests/$_d.Manifest" && cat "$STAGE_MANIFESTS/$_d.Manifest" ; done ) | grep -v '^#' | sort -u -k3 -k2 -k1 >> "$merged_manifest"
	printf '# kmod INDEX for %s/%s\n' "$KARCH" "$KREL" > "$merged_kmod_index" && file_is_writable "$merged_kmod_index" || ! warning "Could not write to merged Manifest at '$merged_kmod_index'!" || return 1
	(cd "$STAGE_KREL" && for _d in * ; do file_exists "$manifests/$_d.kmod-INDEX" && cat "$STAGE_MANIFESTS/$_d.kmod-INDEX"; done ) | grep -v '^#' | sort -u -k3 -k2 -k1  >> "$merged_kmod_index"
	printf '# firmware INDEX for %s/%s\n' "$KARCH" "$KREL" > "$merged_firmware_index" && file_is_writable "$merged_firmware_index" || ! warning "Could not write to merged Manifest at '$merged_firmware_index'!" || return 1
	(cd "$STAGE_KREL" && for _d in * ; do file_exists "$STAGE_MANIFESTS/$_d.firmware-INDEX" && cat "$STAGE_MANIFESTS/$_d.firmware-INDEX" ; done ) | grep -v '^#' | sort -u -k3 -k2 -k1  >> "$merged_firmware_index"
}

kerneltool_depall() {
	local merged="$STAGE_KERN/merged-$KREL"

	# Update module dependencies.
	info_func_set "stage (depmod)"
	msg "Running depmod against staged modules for '$KARCH/$KREL'."
	kerneltool_depmod "$merged" "$KREL" || ! warning "depmod for '$KREL' failed in merged dir '$merged'!" || return 1

	# Create checksummed module and firmware deps for each module.
	info_func_set "stage (kmod-deps)"
	msg "Calculating checksummed module and firmware dependencies for all modules for '$KARCH/$KREL'."
	_moduledepfile="$STAGE_MANIFESTS/$KREL.kmod_fw-DEPS"
	( cd "$merged"
		: > "$_moduledepfile" && file_is_writable "$_moduledepfile" || ! warning "Could not write to module and firmware dependency dump at '$_moduledepfile'!" || return 1
		find "lib/modules" -type f \( -iname '*.ko' -o -iname '*.ko.*' \) -print | while read _mymod; do
			kerneltool_calc_module_fw_deps "$KARCH" "$KREL" "$merged" "$_mymod"
		done
	) >> "$_moduledepfile" || ! warning "Failed to create checkesummed kernel module and firmware dependency dump!" || return 1

}



# Command: depmod
# Usage: kerneltool_depmod <base dir> [<kernel release>] [<subset base>]
kerneltool_depmod() {
	info_prog_set "kerneltool-depmod"
	local _bdir="$1" _krel="$2"
	shift 2
	depmod -a -e ${VERBOSE+-w }-b "${1:-$_bdir}" -F "$_bdir/boot/System.map-$_krel" "$_krel"
	#2>&1 > /dev/null | grep -e '^.+$' >&2 && warning "Errors encounterd by depmod in '$_bdir'!" && return 1
}



# Command: mkmodsubset
# Build module subset given a name and list of module names and/or globs.
# Usage: kerneltool_mkmodsubset <staging base> <arch> <kernel release> <subset name> [<list of modules/globs>...]
kerneltool_mkmodsubset() {
	info_prog_set "kerneltool-mkmodsubset"
	info_func_set "mkmodsubset"
	local stage_base="$1" _arch="$2" _krel="$3" _subname="$4"
	shift 4
	allmods="$@"
	local _merged="$stage_base/$_arch/kernel/merged-$_krel"
	local _modbase="$_merged/lib/modules"
	dir_is_readable "$_modbase" || ! warning "Could not read merged modules directory '$_modbase'" || return 1

	
	local _ddir="$stage_base/$_arch/subsets-$_krel/$_subname"
	dir_not_exists "$_ddir" || rm -rf "$_ddir" || ! Warning "Could not clean destination directory '$_ddir' to build module subeset '$_subname'!" || return 1
	mkdir_is_writable "$_ddir" || ! warning "Could not create destination directory '$_ddir' to build module subset '$_subname'!" || return 1

	local _modout="$_ddir/lib/modules"
	mkdir_is_writable "$_modout" || ! warning "Could not create temp output directory for modules at '$_modout'!" || return 1
	
	local _fwbase="$_merged/lib/firmware"
	local _fwout="$_ddir/lib/firmware"
	
	info_func_set "copy modules"
	local _kmod_index="$stage_base/$_arch/manifests/$_krel.kmod-INDEX"
	local _firmware_index="$stage_base/$_arch/manifests/$_krel.firmware-INDEX"
	local _kmod_fw_deps="$stage_base/$_arch/manifests/$_krel.kmod_fw-DEPS"
	local mod_subset_selected mod_subset_alldeps mod_subset_manifest
	mod_subset_selected="$_ddir.kmod-Selected"
	mod_subset_alldeps="$_ddir.kmod_fw-DEPS"
	mod_subset_modfwlist="$_ddir.kmod_fw-LIST"
	mod_subset_manifest="$_ddir.kmod-Manifest"
	mod_subset_modindex="$_ddir.kmod-INDEX"
	mod_subset_fwindex="$_ddir.firmware-INDEX"
		

	# Select modules against kmod index:
	if [ $# -gt 0 ] ; then
		msg "Selecting requested modules and their deps from '$_modbase' into '$mod_subset_manifest'."
		local _kmod _kpkg _kfsum _kfile
		cat "$_kmod_index" | while read _kmod _kpkg _kfsum _kfile ; do
			for _mod in $allmods ; do
				#msg "_kmod: $_kmod"
				#msg "_file: $_kfile"
				#msg "_mod:$_mod"
				case "$_mod" in 
					*/|*/*/) case "$_kfile" in lib/modules/*/$_mod* ) : ;; *) continue;; esac ;;
					*/*) case "$_kfile" in lib/modules/*/$_mod ) : ;; *) continue ;; esac ;;
					*) case "$_kmod" in "kmod:$_mod:"* | "kmod:${_mod%.ko*}:"* ) : ;; *) continue ;; esac ;;
				esac
				printf "$_kmod\n"
			done
		done > "$mod_subset_selected"
		# Select all deps:
		kerneltool_moddep_helper "$_kmod_fw_deps" "$mod_subset_selected" > "$mod_subset_alldeps"
	else
		msg "Selecting all staged modules found in '$_modbase' and their firmware deps into '$mod_subset_alldeps'."
		cat "$_kmod_fw_deps" | grep -v -e '^#' -e '^$' | sort -u > "$mod_subset_alldeps"
	fi

	# Created flattened list of all modules and firmware to be included.
	cat "$mod_subset_alldeps" | tr -s '\t ' '\n' | grep -v -e '^$' -e '^#' | sort -u > "$mod_subset_modfwlist"

	# Generate complete index from flattened dep list for both modules and firmware.
	file_is_readable "$_kmod_index" || ! warning "Could not read kernel module index '$_kmod_index'!" || return 1
	grep -h -F -f "$mod_subset_modfwlist" "$_kmod_index" > "$mod_subset_modindex"
	if grep -e 'firmware:' -q "$mod_subset_alldeps" ; then
		file_is_readable "$_firmware_index" || ! warning "Firmware indicated in deps, but could not read firmware index '$_firmware_index'!" || return 1
		grep -h -F -f "$mod_subset_modfwlist" "$_firmware_index" > "$mod_subset_fwindex"
	else unset mod_subset_fwindex ; fi

	# Extract manifest from subset mod index and fw index. 
	cut -f 2,3,4 "$mod_subset_modindex" ${mod_subset_fwindex:+"$mod_subset_fwindex"} > "$mod_subset_manifest"
	
	# Copy modules from list using subset manifest
	info_func_set "copy"
	msg "Copying modules and fimrware included in manifest '$mod_subset_manifest' from '$_bdir' to '$_ddir'."
	file_exists "$mod_subset_manifest" || ! warning "No module-subset Manifest found at '$mod_subset_manifest'!" || return 1
	_bdir="$stage_base/$_arch/$_krel"
	kerneltool_manifest_copy "$mod_subset_manifest" "$_bdir" "$_ddir" || ! warning "Failed to copy files in manifest '$mod_subset_manifest' from '$_merged' to '$_ddir'!" || return 1

	# Copy module.* metadata files from source to dest.
	cp -L "$_modbase/$_krel"/modules.* "$_modout/$_krel/" || ! warning "Could not copy module metadata from '$_modbase/$_krel' to '$_modout/$_krel'!" || return 1

	# Run depmod against dest.
	kerneltool_depmod "$_merged" "$_krel" "$_ddir"

}

kerneltool_moddep_helper() {
	local _alldeps="$1" _selected="$2"
	# Select subset deps:
	awk -e '
		BEGIN { fcount = 0 ; fname = "" }
		fname == "" { fname = FILENAME }
		FILENAME != fname { fname = FILENAME ; fcount++ }
		fcount == 0 { deps[$1] = $0 }
		fcount > 0 { req[$1] = $0 }
		function allsubdeps(ad, d, sd, td, t) {
			split(ad[d], td)
			for ( t in td ) {
				sd[td[t]] = td[t]
				if (t != d) { allsubdeps(ad, t, sd) }
			}		
		}
		END {
			for (dep in req) { allsubdeps(deps, dep, subdeps) }
			for (dep in subdeps) { print deps[dep] } 
		}
		' "$_alldeps" "$_selected" | sort -u
}

kerneltool_manifest_copy() {
	local _manifest="$1" _bdir="$2" _ddir="$3"
	file_is_readable "$_manifest" || ! warning "Manifest file '$_manifest' is not readable!" || return 1
	dir_is_readable "$_bdir" || ! warning "Source directory '$_bdir' is not readable!" || return 1
	mkdir_is_writable "$_ddir" || ! warning "Could not create writable destination directory at '$_ddir'!" || return 1
	while IFS=$'\t ' read _pkg _fsum _file ; do
		if [ "$_file" ] ; then 
			_filein="$_bdir/${_pkg#*:$_arch/}/$_file"
			_fileout="$_ddir/$_file"
			[ "$VERBOSE" ] && msg "'$_filein' -> '$_fileout'"
			file_exists "$_filein" || ! warning "Could not read file '$_filein'!" || continue
			mkdir_is_writable "${_fileout%/*}" || ! warning "Could not make output subdirectory '${_fileout%/*}'!" || return 1
			cp -l -f "$_filein" "$_fileout" || cp -f "$_filein" "$_fileout" || ! warning "Could not hardlink or copy '$_filein' to '$_fileout'!" || return 1
			echo "$_file"
		fi
	done < "$_manifest" | wc -l | ( read _count && msg "Copied $_count files from '$_bdir' to '$_ddir'." )
	return 0
}



# Command: mkmodarchive
# Build modarchive from subset or list of modules.
# Usage: kerneltool_mkmodarchive <staging base> <arch> <kernel release> <output file> [modsubset=<subset name>] [<list of modules/globs>...]
kerneltool_mkmodarchive() {
	info_prog_set "kerneltool-mkmodarchive"
	info_func_set "mkmodarchive"
	local stage_base="$1" _arch="$2" _krel="$3" _out="$4" _subname="$5"
	shift 4
	local _outdir="${_out%/*}" _outname="${_out##*/}"

	# Clean up any stale output files and make sure we can write to our output directory
	mkdir_is_writable "$_outdir" || ! warning "Could not create writable output directory '$_outdir'!" || return 1
	! file_exists "$_out" || rm -f "$_out" || ! warning "Could not remove existing output file '$_outname' in '$_outdir'!" || return 1
	: > "$_out" && file_is_writable "$_out" && rm -f "$_out" || ! warning "Could not create output file '$_outname' in '$_outdir'!" || return 1

	local _tmp _keeptmp _subname _subexists
	case "$_subname" in
		modsubset=*)
			_keeptmp="yes"
			_subname="${_subname#*=}"
			shift
			;;
		*)
			_keeptmp="no"
			_subname="modloop-$_krel-$(echo "$*" | md5sum | cut -d' ' -f 1)"
			;;
	esac

	local _tmp="$stage_base/$_arch/kernel/subsets-$_krel/$_subname"

	[ "$_keeptmp" = "yes" ] && dir_exists "$_tmp" || kerneltool_mkmodsubset "$stage_base" "$_arch" "$_krel" "$_subname" $@ || ! warning "Failed to make module subset '$_subname' needed to make '$_outname'!" || return 1


	# Determine how to build the output based on the filename extension. Unknown extensions are assumed to be squashfs.
	case "$_outname" in
		*.cpio)
			info_func_set "create cpio"
			(cd "$_tmp" && find | sort -u | sed -e 's|\./||g' | cpio -R 0:0 -H newc -o ) > "$_outdir/$_outname" || ! warning "Failed to create cpio archive '$_outname' in '$_outdir'!" || return 1
			msg "Module cpio archive for '$_arch/$_krel' is at '$_outdir/$_outname'."
			;;
		*.cpio.gz)
			info_func_set "create cpio.gz"
			(cd "$_tmp" && find | sort -u | sed -e 's|\./||g' | cpio -R 0:0 -H newc -o | gzip -9 ) > "$_outdir/$_outname" || ! warning "Failed to create compressed cpio archive '$_outname' in '$_outdir'!" || return 1
			msg "Created gz compressed module cpio archive '$_outname' for '$_arch/$_krel' in '$_outdir'."
			;;
		*.tar)
			info_func_set "create tarball"
			(cd "$_tmp" && tar --owner=0:0 --numeric-owner -cf "$_out" * ) || ! warning "Failed to create '$_outname' in '$_outdir'!" || return 1
			msg "Created module tarball '$_outname' for '$_arch/$_krel' in '$_outdir'."
			;;
		*.tar.*)
			info_func_set "create tarball.${_outname##*.}"
			(cd "$_tmp" && tar --owner=0:0 --numeric-owner -caf "$_out" * ) || ! warning "Failed to create '$_outname' in '$_outdir'!" || return 1
			msg "Created ${_outname##*.} compressed module tarball '$_outname' for '$_arch/$_krel' in '$_outdir'."
			;;
		*)
			info_func_set "create squashfs"
			mksquashfs "$_tmp/lib" "$_out" -root-owned -no-recovery -noappend -progress -comp xz -exit-on-error ${VERBOSE:+-info} | cat ${QUIET:+/dev/null} \
				|| ! warning "Failed to mksqwashfs '$_tmp/lib' to '$_out' with compression 'xz'!" || return 1
			msg "Created xz compressed squashfs modloop file '$_outname' for '$_arch/$_krel' in '$_outdir'."
			;;
	esac

	# Clean up our subset tmpdir if needed.
	[ "$_keeptmp" = "yes" ] || rm -rf  "$_tmp" || ! warning "Could not clean up tmp directory for mkmodarchiveat '$_tmp'!" || return 1

	msg "mkmodarchive complete."
	return 0
}



# Command: mkmodloop
# Build modloop (rooted at lib/) from subset or list of modules.
# Usage: kerneltool_mkmodloop <staging base> <arch> <kernel release> [modsubset=<subset name>] [<list of modules/globs>...]
kerneltool_mkmodloop() {
	local stage_base="$1" _arch="$2" _krel="$3"
	shift 3
	local _outdir="$stage_base/$_arch/out-$_krel/boot"
	local _outname="modloop-$_krel"
	kerneltool_mkmodarchive "$stage_base" "$_arch" "$_krel" "$_outdir/$_outname" $@
}



# Command: mkmodcpio
# Build module cpio archive from subset or list of modules.
# Usage: kerneltool_mkmodarchive <staging base> <arch> <kernel release> [modsubset=<subset name>] [<list of modules/globs>...]
kerneltool_mkmodcpio() {
	local stage_base="$1" _arch="$2" _krel="$3"
	shift 3
	case "$1" in modsubset=*) local _subset="${1#modsubset=}" ;; esac
	local _outdir="$stage_base/$_arch/out-$_krel/boot"
	local _outname="modules-${_subset:+$_subset-}$_krel.cpio"
	kerneltool_mkmodarchive "$stage_base" "$_arch" "$_krel" "$_outdir/$_outname" $@
}



# Command: mkmodcpiogz
# Build compressed cpio archive from subset or list of modules.
# Usage: kerneltool_mkmodarchive <staging base> <arch> <kernel release> [modsubset=<subset name>] [<list of modules/globs>...]
kerneltool_mkmodcpiogz() {
	local stage_base="$1" _arch="$2" _krel="$3"
	shift 3
	case "$1" in modsubset=*) local _subset="${1#modsubset=}" ;; esac
	local _outdir="$stage_base/$_arch/out-$_krel/boot"
	local _outname="modules-${_subset:+$_subset-}$_krel.cpio.gz"
	kerneltool_mkmodarchive "$stage_base" "$_arch" "$_krel" "$_outdir/$_outname" $@
}



###
##  Kerneltool Helper Functions
###

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


# Get sha512 checksum of uncompressed module given a compressed or uncompressed module, or checksum of raw file (firmware) with no decompression applied.
# Usage: kerneltool_checksum_module <kernel module file>
kerneltool_checksum_module() {
	local _file="$1" _mc
	file_is_readable "$1" || ! warning "Could not read module '$1'!" || return 1
	case "$_file" in
		*.ko) : uncompressed module; _mc='cat';;
		*.ko.bz*) : compressed module; _mc='bzcat';; *.ko.gz)_mc='zcat';; *.ko.lz4)_mc='lz4cat';; *.ko.lzo*)_mc='lzopcat';; *.ko.xz|*.ko.lz*)_mc='xzcat';;
		*) : unrecognized module extension, checksum raw file ; _mc='cat' ;;
	esac
	$_mc "$_file" | sha512sum | cut -d' ' -f 1 | sed 's/^/sha512:/g'
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

		done

		# Find list of required firmware for each module, if any.
		_fw="$(modinfo -b "$_bdir" -k "$_krel" -F firmware "$_myfile" )"
		for _myfw in $_fw ; do
			_file="$_fwbase/$_myfw"
			if file_is_readable "$_file" ; then 
				_sums="${_sums}$(printf 'firmware:%s:%s:%s\t' "$_myfw" "$_arch" "$(kerneltool_checksum_module "$_file")" )"
				[ "$VERBOSE" ] && msg "Module '$_mymod' depends on '$_myfw'."
			else 
				_sums="${_sums}$(printf 'firmware:%s:%s:%s\t' "$_myfw" "$_arch" "UNRESOLVED" )"
				[ "$VERBOSE" ] && msg "Could not find firmware '$_myfw' in '$_fwbase' needed by module '$_mymod'!"
			fi
		done
		printf '%s\n' "$_sums"
	done
}

