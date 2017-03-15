plugin_profiles() {
	return 0
}



build_profile() {
	info_func_set "build profile_$PROFILE"
	local output_file _dir

	# Reset status vars for each profile run.
	_my_sections=""
	_my_dirs=""
	_dirty="no"
	_fail="no"

	# Evaluate profile using current environment. Bail if it doesn't support our arch.
	profile_$PROFILE
	var_list_has archs $ARCH || return 0	

	msg "Building $PROFILE"

	# Collect list of needed sections, and make sure they are built

	for SECTION in $all_sections; do
		info_func_set "build $SECTION"
			section_$SECTION || ( warning "Section '$SECTION' returned failure!" && return 1 ) || return 1
	done
	[ "$_fail" = "no" ] || return 1
	info_func_set "build profile_$PROFILE"

	# Defaults
	[ -n "$image_name" ] || image_name="alpine-${PROFILE}"
	[ -n "$output_filename" ] || output_filename="${image_name}-${RELEASE}-${ARCH}.${image_ext}"
	output_file="${OUTDIR:-.}/$output_filename"

	# Construct final image
	info_func_set "build profile_${PROFILE}:image"
	local _imgid=$(printf_n $image_name $RELEASE $build_date $_my_sections $_my_dirs | sort -u | checksum)
	DESTDIR="$WORKDIR/image-$_imgid-$ARCH-$PROFILE"
	[ ! -e "$DESTDIR" ] && _dirty="yes"

	if [ "$_dirty" = "yes" ]; then
		msg "Merging sections '$_my_sections' into '$DESTDIR':"
		if [ -z "$_simulate" ]; then
			# Merge sections
			rm -rf "$DESTDIR"
			mkdir -p "$DESTDIR"
			for _dir in $_my_dirs; do
				local d="$WORKDIR/${_dir##/}"
				[ -d "$d" ] && [ "$(echo "$d"/*)" != "$d/*" ] || continue
				for _fn in "$d"/*; do
					_fn="$(realpath $_fn)"
					[ ! -e "$_fn" ] || msg2 " * $_fn" && cp -Lrs $_fn $DESTDIR/
				done
			done
			echo "${image_name}-${RELEASE} ${build_date}" > "$DESTDIR"/.alpine-release
		fi
	fi

	if [ "$_dirty" = "yes" -o ! -e "$output_file" ]; then
		msg "Creating '$output_filename':"
		# Create image
		[ -n "$output_format" ] || output_format="${image_ext//[:\.]/}"
		create_image_${output_format} || { _fail="yes"; false; }

		if [ "$_checksum" = "yes" ]; then
			msg "Calculating checksums:"
			for _c in $all_checksums; do
				local _mysum="$(${_c}sum "$output_file" | cut -d' '  -f1)  ${output_filename}"
				echo $_mysum > "${output_file}.${_c}"
				msg2 "  - $_c: $_mysum"
			done
		fi

		if [ -n "$_yaml_out" ]; then
			msg "Creating '$_yaml_out' for release '$RELEASE':"
			$mkimage_yaml --release $RELEASE \
				"$output_file" >> "$_yaml_out"
		fi
	fi

}


# Empty plugin to keep load_plugins happy.
plugin_sections() {
	return 0
}

build_section() {
	local section="$1"
	shift
	local args="$@"
	local _dir="$(printf '%s-%s' "$section-$(echo ${args//[^a-zA-Z0-9]/_} | cut -c1-10)" "$(echo $args | checksum)" )" && _dir="${_dir#/}"
	local args="$@"

	info_func_set "build section_$section"

	if [ -z "$_dir" ]; then
		_fail="yes"
		warning "Building '$section' failed: '$_dir' empty!"
		return 1
	fi

	if [ ! -e "$WORKDIR/${_dir}" ]; then
		DESTDIR="$WORKDIR/${_dir}.work"
		msg "--> $section $args"

		msg "Building section '$section' with args '$args':"
		if [ -z "$_simulate" ]; then
			rm -rf "$DESTDIR"
			mkdir -p "$DESTDIR"
			if build_${section} "$@"; then
				mv "$DESTDIR" "$WORKDIR/${_dir}"
				_dirty="yes"
				msg "Built '$section' -> '${_dir}'"
			else
				_fail="yes"
				warning "Building '$section' failed!"
				rm -rf "$DESTDIR"
				return 1
			fi
		fi
	fi

	# Add this directory and section to built list.
	var_list_add _my_dirs "${_dir}"
	var_list_add _my_sections "$section"

	unset DESTDIR
}
