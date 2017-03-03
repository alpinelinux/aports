
plugin_overlays() {
	var_list_alias overlays
}

section_overlays() {
	[ "$overlays" ] || return 0
	local _id="$(echo "$hostname::$overlays" | checksum)"
	[ "$overlays" ] && build_section overlays $hostname $local_id
}

build_overlays() {
	local my_overlays="$overlays"
	local run_overlays=""
	ovl_hostname="$1"
	ovl_root_dir="$DESTDIR/ovlroot"
	mkdir -p $ovl_root_dir

	fkrt_faked_start $ovl_fkrt_inst
	
	local watchdog=20
	while [ "${my_overlays## }" ] && [ $watchdog -gt 0 ]; do
		for _overlay in $my_overlays ; do
			overlay_${_overlay}
			if ( list_has_any "$_conflicts" "$overlays" ) ; then
				warning "overlay: Overlay conflict detected!"
				warning "    '$_overlay' conflicts with '$(list_filter "$_conflicts" "$overlays" )'"
				fkrt_faked_stop $ovl_fkrt_inst
				return 1
			fi
			if ( list_has_all "$_needs" "$overlays" ) ; then
				list_has_any "$_after" "$my_overlays" && continue
				list_has_all "$(list_filter "$_before" "$overlays")" "$run_overlays" || continue
				local _func
				for _func in $_call ; do
					[ "$(type -t $_func)" ] && $_func
				done
				run_overlays="$run_overlays $_overlay"
				var_list_del my_overlays "$_overlay"

				watchdog=$(( $watchdog + 5 ))
			else
				if ( list_has_all "$_needs" "$all_overlays" ) ; then
					list_add "$_needs" "$my_overlays"
				else
					warning "Could not find the following overlays needed by overlay '$_overlay':"
					warning "$(list_del "$all_overlays" "$_needs" )"
					fkrt_faked_stop $ovl_fkrt_inst
					return 1
				fi
			fi
			unset _needs
			unset _conflicts
			unset _before
			unset _after
			unset run_ovl
		done

		watchdog=$(( $watchdog - 1 ))
		if [ $watchdog -lt 1 ] ; then
			warning "overlay: Watchdog counter expired!"
			warning "    This means we probably got stuck in an infinate loop resolving run order."
			warning "    The following were not resolved: $my_overlays"
			fkrt_faked_stop $ovl_fkrt_inst
			return 1
		fi
	done
	
	[ "$run_overlays" ] && ovl_targz_create $ovl_hostname $ovl_root_dir

	fkrt_faked_stop $ovl_fkrt_inst

	return 0
}

ovl_fkrt_enable() {
	fkrt_enable $ovl_fkrt_inst
}

ovl_fkrt_disable() {
	fkrt_disable
}

ovl_get_root() {
	printf %s $ovl_root_dir
}

ovl_path() {
	printf "%s" "${ovl_root_dir%%//}${1##/}"
}



# Usage: targz_dir <output.tar.gz> <source directory> [<source objects>]
targz_dir() {
	local output_file="$1"
	local source_dir="$2"
	shift 2

	tar -c --numeric-owner --acls --xattrs -C "$source_dir" "$@" | gzip -9n > "$output_file"
}

ovl_targz_create() {
	local _name="$1"
	local _dir="$2"
	shift 2

	ovl_fkrt_enable
	(	targz_dir "${DESTDIR%/}/${_name##*/}.apkovl.tar.gz" "$_dir" "$@" )
	ovl_fkrt_disable
}


ovl_create_file() {
	local owner perms file filename filedir
	owner="$1"
	perms="$2"
	file="${3}"
	file="$(ovl_get_root)/${file#/}"
	filename="${file##*/}"
	filedir="${file%$filename}"

	ovl_fkrt_enable
	(	mkdir -p "$filedir"
		cat > "${filedir%/}/$filename"
		chown "$owner" "$file"
		chmod "$perms" "$file"
	)
	ovl_fkrt_disable
}


# Directories and links to be treated as directories must be specified with trailing slash
# Files and links to be created or overwritten without trailing slash
# i.e. ovl_ln /usr/include/ /opt/include
ovl_create_link() {
	local target targetname targetdir link linkname linkdir

	target="$1"
	targetname="${target##*/}"
	targetdir="${target%$targetname}"
	targetdir="${targetdir%/}"

	link="$2"
	linkname="${link##*/}"
	linkdir="${link%$linkname}"
	linkdir="${linkdir%/}"

	# If only link name given, create link with that name in target directory.
	[ "$linkdir" ] || linkdir="$targetdir"

	# If only link directory given, use target name as link name.
	[ "$linkname" ] || linkname="$targetname"

	# If both arguments are directories, use last portion of target directory as link name.
	[ ! "$targetname" ] && [ ! "$linkname" ] && linkname="${targetdir##*/}"

	# If neither argument has a directory component still, check if $PWD is within the overlay, othewise bail.
	if [ ! "$targetdir" ] && [ ! "$linkdir" ] ; then
		local mypwd="$(realpath "$PWD")"
		if [ "${mypwd#$(ovl_get_root)}" != "$mypwd" ] ; then 
			linkdir="${mypwd%/}"
		else
			warning "ovl_ln called with argument that could not be resolved: target:'$1' link:'$2'."
			return 1
		fi
	else
		# Prepend overlay root to linkdir.
		linkdir="$(ovl_get_root)/${linkdir#/}"
	fi

	# Build our link:

	link="${linkdir%/}/$linkname"
	

	ovl_fkrt_enable 
	(	mkdir -p "$linkdir"
		ln -sfT "${target%/}" "$link"	
	)
	ovl_fkrt_disable
}


ovl_cat() {
	ovl_fkrt_enable
	( printf "%s\n" "$@" | sed -e "/^-$/b; s/^/$(ovl_get_root)" | xargs cat )
	ovl_fkrt_disable
}


ovl_mkdir() {
	local opts
	opts=""
	while [ "$1" != "${1#-}" ] ; do opts="$opts $1" ; shift ; done

	ovl_fkrt_enable
	(	printf "$(ovl_get_root)/%s\\n" "${@#/}" | xargs mkdir $opts )
	ovl_fkrt_disable
}


ovl_rm() {
	local opts
	opts=""
	while [ "$1" != "${1#-}" ] ; do opts="$opts $1" ; shift ; done

	ovl_fkrt_enable
	(	printf "$(ovl_get_root)/%s\\n" "${@#/}" | xargs rm $opts )
	ovl_fkrt_disable
}


ovl_ln() {
	local output outdir opts

	output="$(ovl_get_root)/$(eval "echo \${$##/}")"
	outdir="${outdir%${outdir##*/}}"

	opts=""
	while [ "$1" != "${1#-}" ] ; do opts="$opts $1" ; shift ; done

	ovl_fkrt_enable
	(	mkdir -p "$outdir"
		printf "$(ovl_get_root)/%s\\n" "${@#/}" | xargs ln $opts
	)
	ovl_fkrt_disable

}


ovl_cp() {
	local output outdir opts

	output="$(ovl_get_root)/$(eval "echo \${$##/}")"
	outdir="${outdir%${outdir##*/}}"

	opts=""
	while [ "$1" != "${1#-}" ] ; do opts="$opts $1" ; shift ; done

	ovl_fkrt_enable
	(	mkdir -p "$outdir"
		printf "$(ovl_get_root)/%s\\n" "${@#/}" | xargs cp $opts
	)
	ovl_fkrt_disable

}


ovl_mv() {
	local output outdir opts

	output="$(ovl_get_root)/$(eval "echo \${$##/}")"
	outdir="${outdir%${outdir##*/}}"

	opts=""
	while [ "$1" != "${1#-}" ] ; do opts="$opts $1" ; shift ; done


	ovl_fkrt_enable
	(	mkdir -p "$outdir"
		printf "$(ovl_get_root)/%s\\n" "${@#/}" | xargs mv $opts
	)
	ovl_fkrt_disable
}


ovl_chown() {
	local opts owner
	opts=""
	while [ "$1" != "${1#-}" ] ; do opts="$opts $1" ; shift ; done

	owner="$1"
	shift

	ovl_fkrt_enable
	(	printf "$(ovl_get_root)/%s\\n" "${@#/}" | xargs mkdir $opts "$owner" )
	ovl_fkrt_disable
}


ovl_chmod() {
	local opts perms
	opts=""
	while [ "$1" != "${1#-}" ] ; do opts="$opts $1" ; shift ; done
	
	perms="$1"
	shift

	ovl_fkrt_enable
	( 	printf "$(ovl_get_root)/%s\\n" "${@#/}" | xargs mkdir $opts $perms )
	ovl_fkrt_disable
}

ovl_runlevel_add() {
	local _lvl="$1"
	local _srv
	shift
	for _srv in "$@" ; do
		ln -sfT "/etc/init.d/$1" "$(ovl_get_root)/etc/runlevels/$2/$1"
	done
}
