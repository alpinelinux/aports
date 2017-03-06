plugin_profiles() {
	var_list_alias host_apks
	var_list_alias apks
	var_list_alias apks_flavored
	var_list_alias arch
	var_list_alias initfs_apks
	var_list_alias initfs_apks_flavored
	var_list_alias initfs_only_apks
	var_list_alias initfs_only_apks_flavored
	var_list_alias initfs_features
	var_list_alias kernel_flavors
}



build_profile() {
	local _id _dir _spec
	_my_sections=""
	_dirty="no"
	_fail="no"

	profile_$PROFILE
	list_has $ARCH $arch || return 0

	info_func_set "build"

	msg "Building $PROFILE"

	# Collect list of needed sections, and make sure they are built
	for SECTION in $all_sections; do
		section_$SECTION || return 1
	done
	[ "$_fail" = "no" ] || return 1

	# Defaults
	[ -n "$image_name" ] || image_name="alpine-${PROFILE}"
	[ -n "$output_filename" ] || output_filename="${image_name}-${RELEASE}-${ARCH}.${image_ext}"
	local output_file="${OUTDIR:-.}/$output_filename"

	# Construct final image
	local _imgid=$(echo -n $_my_sections | sort | checksum)
	DESTDIR=$WORKDIR/image-$_imgid-$ARCH-$PROFILE
	if [ "$_dirty" = "yes" -o ! -e "$DESTDIR" ]; then
		msg "Creating $output_filename"
		if [ -z "$_simulate" ]; then
			# Merge sections
			rm -rf "$DESTDIR"
			mkdir -p "$DESTDIR"
			for _dir in $_my_sections; do
				for _fn in $WORKDIR/$_dir/*; do
					[ ! -e "$_fn" ] || cp -Lrs $_fn $DESTDIR/
				done
			done
			echo "${image_name}-${RELEASE} ${build_date}" > "$DESTDIR"/.alpine-release
		fi
	fi

	if [ "$_dirty" = "yes" -o ! -e "$output_file" ]; then
		# Create image
		[ -n "$output_format" ] || output_format="${image_ext//[:\.]/}"
		create_image_${output_format} || { _fail="yes"; false; }

		if [ "$_checksum" = "yes" ]; then
			for _c in $all_checksums; do
				echo "$(${_c}sum "$output_file" | cut -d' '  -f1)  ${output_filename}" > "${output_file}.${_c}"
			done
		fi

		if [ -n "$_yaml_out" ]; then
			$mkimage_yaml --release $RELEASE \
				"$output_file" >> "$_yaml_out"
		fi
	fi

	info_func_set ""
}


# Empty plugin to keep load_plugins happy.
plugin_sections() {
	return 0
}

build_section() {
	local section="$1"
	local args="$@"
	local _dir="${args//[^a-zA-Z0-9]/_}"
	shift
	local args="$@"

	if [ -z "$_dir" ]; then
		_fail="yes"
		return 1
	fi

	if [ ! -e "$WORKDIR/${_dir}" ]; then
		DESTDIR="$WORKDIR/${_dir}.work"
		msg "--> $section $args"

		info_func_set "build_$section"
		msg "Building section '$section' with args '$args':"
		if [ -z "$_simulate" ]; then
			rm -rf "$DESTDIR"
			mkdir -p "$DESTDIR"
			if build_${section} "$@"; then
				mv "$DESTDIR" "$WORKDIR/${_dir}"
				_dirty="yes"
			else
				rm -rf "$DESTDIR"
				_fail="yes"
				info_func_set ""
				return 1
			fi
		fi
	fi
	unset DESTDIR
	all_dirs="$all_dirs $_dir"
	_my_sections="$_my_sections $_dir"
	info_func_set ""
}



# Helpers to defined kernel-flavor suffixes to all specified roots.
suffix_kernel_flavor() {
	local _f="$1"
	shift
	for _a in $@; do
		_s="$s $_a-$_f"
	done
	echo $_s
}

suffix_all_kernel_flavors() {
	local _f _a _s=""
	for _f in $kernel_flavors; do
		suffix_kernel_flavor $_f $@
	done
	echo $_s
}

section_kernels() {
	local _f _a _pkgs
	for _f in $kernel_flavors; do
		#FIXME Should these really be hard-coded?
		var_list_set _pkgs "linux-$_f linux-firmware"

		var_list_add _pkgs "$initfs_apks $initfs_only_apks"
		var_list_add _pkgs "$(suffix_kernel_flavor $_f $initfs_apks_flavored $initfs_only_apks_flavored)"

		local id=$( (echo "$initfs_features::$_hostkeys" ; $APK fetch --root "$APKROOT" --simulate alpine-base $_pkgs | sort) | checksum)
		build_section kernel $ARCH $_f $id $_pkgs
	done
}

build_kernel() {
	local _flavor="$2"
	shift 3
	local _pkgs="$@"
	update-kernel \
		$_hostkeys \
		${_abuild_pubkey:+--apk-pubkey $_abuild_pubkey} \
		--media \
		--flavor "$_flavor" \
		--arch "$ARCH" \
		--package "$_pkgs" \
		--feature "$initfs_features" \
		--repositories-file "$APKROOT/etc/apk/repositories" \
		"$DESTDIR"
}

# Local apk repository support.
section_apks() {
	# Build list of all required apks
	add_apks "$(suffix_all_kernel_flavors $apks_flavored) $initfs_apks $(suffix_all_kernel_flavors $initfs_apks_flavored)"
	[ -n "$apks" ] || return 0

	build_section apks $ARCH $($APK fetch --root "$APKROOT" --simulate --recursive $apks | sort | checksum)
}

build_apks() {
	local _apksdir="$DESTDIR/apks"
	local _archdir="$_apksdir/$ARCH"
	mkdir -p "$_archdir"

	$APK fetch --root "$APKROOT" --link --recursive --output "$_archdir" $apks
	if ! ls "$_archdir"/*.apk >& /dev/null; then
		return 1
	fi

	$APK index \
		--description "$RELEASE" \
		--rewrite-arch "$ARCH" \
		--index "$_archdir"/APKINDEX.tar.gz \
		--output "$_archdir"/APKINDEX.tar.gz \
		"$_archdir"/*.apk
	abuild-sign "$_archdir"/APKINDEX.tar.gz
	touch "$_apksdir/.boot_repository"
}

