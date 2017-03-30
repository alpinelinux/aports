

# Usage: mkinitfs_main <source dir> <temp dir> <outfile> <kernel version> <initfs-features...>
mkinitfs_main() {
	local _src _tgt _out _kver _err
	_src="${1%/}"
	_tgt="${2%/}"
	_out="${3%/}"
	_kver="${4}"
	shift 4

	initfs_file_root="${initfs_file_root:-/usr/share/mkinitfs}"
	initfs_src_fstab="${initfs_src_fstab:-$initfs_file_root/fstab}"
	initfs_src_init="${initfs_src_init:-$initfs_file_root/initramfs-init}"
	initfs_src_passwd="${initfs_src_passwd:-$initfs_file_root/passwd}"
	initfs_src_group="${initfs_src_group:-$initfs_file_root/group}"

	_err=1
	mkinitfs_initfs_base "$_src" "$_tgt" "$@" \
		&& if [ "$_mkinitfs_nomods" != "yes" ] ; then \
			mkinitfs_initfs_kmods "$_src" "$_tgt" "$_kver" "$@" \
			&& mkinitfs_initfs_firmware "$_src" "$_tgt" "$_kver" "$@" ; \
		fi \
		&& mkinitfs_initfs_apk_keys "$_src" "$_tgt" \
		&& mkinitfs_initfs_cpio "$_tgt" "$_out" \
		&& _err=0

	[ $_err -ne 0 ] && warning "mkintfs: Building initfs failed!"
	return $_err
}

# Usage: mkinitfs_feature_files <source> <type> <features...>
mkinitfs_feature_files() {
	local _src="${1%/}"
	local _type="$2"
	shift 2

	local f glob file fname
	for f in $@; do
		fname="$(type "_initfs_${f//-/_}_${_type}")"
		[ "$fname" != "${fname//shell function/}" ] \
			|| ! warning "mkinitfs: Could not get $_type list for feature '$f'!" \
			|| continue

		for glob in $( _initfs_${f//-/_}_${_type} | sed -e '/^$/d' -e '/^#/d' -e "s|^/*|${_src%/}/|") ; do
			for file in $glob; do
				if [ -d $file ]; then
					find $file -type f
				elif [ -e "$file" ]; then
					echo $file
				fi
			done
		done
	done
}

# Usage: mkinitfs_find_files <source> <features...>
mkinitfs_find_files() {
	local _src
	_src="${1%/}"
	shift

	( cd $_src && set -- $(mkinitfs_feature_files "$_src" files "$@") && [ $# -lt 1 ] && return 0 || lddtree -R "$_src" -l --no-auto-root $@ | sort -u ) || return 1
	return 0
}

# Usage: mkinitfs_initfs_base <source> <target> <features...>
mkinitfs_initfs_base() {
	local _src _tgt
	_src="${1%/}"
	_tgt="${2%/}"
	shift 2

	local i=
	for i in dev proc sys sbin bin run .modloop lib/modules media/cdrom \
	    etc/apk media/floppy media/usb newroot; do
		mkdir_is_writable "$_tgt/$i"
	done


	mkinitfs_find_files | sed -e "s|^$_src||" | (cd $_src && cpio --quiet -pdm "$_tgt") || return 1
	
	# copy init
	install -m755 "$initfs_src_init" "$_tgt"/init || return 1
	for i in "$initfs_src_fstab" "$initfs_src_passwd" "$initfs_src_group"; do
		install -Dm644 "$i" "$_tgt"/etc/${i##*/} || return 1
	done
}

# Usage: ... | mkinitfs_find_kmod_deps <source> <kernel version>
mkinitfs_find_kmod_deps() {
	local _src
	_src="${1%/}"
	_kver="${2%}"
	shift 2

	awk -v prepend="/lib/modules/$_kver/" -v modulesdep="${_src}/lib/modules/$_kver/modules.dep" '
function recursedeps(k,		j, dep) {
	if (k in visited)
		return;
	visited[k] = 1;
	split(deps[k], dep, " ");
	for (j in dep)
		recursedeps(dep[j]);
	print(prepend k);
}

BEGIN {
	if (modulesdep == "")
		modulesdep="modules.dep";
	FS = ": ";
	while ( (getline < modulesdep) > 0) {
		if (substr($0,1,1) == "/") {
			gsub(prepend, "", $1);
			gsub(prepend, "", $2);
		}
		deps[$1] = $2;
	}
}

{
	mod[$0] = 1;
}

END {
	for (i in mod)
		recursedeps(i);
}'
}

# Usage: mkinitfs_find_kmods <source> <kernel version> <features...>
mkinitfs_find_kmods() {
	local _src _kver _modsrc
	_src="${1%/}"
	_kver="${2%/}"
	shift 2
	_modsrc="$_src/lib/modules/$_kver"

	mkinitfs_feature_files "${_modsrc}" modules "$@" \
		| sed -e "s|^${_modsrc}/||g" | (cd $_modsrc && mkinitfs_find_kmod_deps "$_src" "$_kver") \
		| sed -e "s|^/*|${_src}/|g" || return 1

}

# Usage: mkinitfs_initfs_kmods <source> <target> <kernel version> <features...>
mkinitfs_initfs_kmods() {
	local _src _tgt _kver
	_src="${1%/}"
	_tgt="${2%/}"
	_kver="${3}"
	shift 3

	local file=
	local kerneldir="$_src/lib/modules/$_kver"

	rm -rf "$_tgt"/lib/modules
	mkdir_is_writable "$_tgt/lib/modules/$_kver"
	# make sure we have modules.dep
	if ! [ -f "$kerneldir"/modules.dep ]; then
		depmod -b "$_src" $kernel
	fi

	(cd "$_src" && mkinitfs_find_kmods "$_src" "$_kver" "$@" | sed -e "s|^${_src}/||g" | sort -u | cpio --quiet -pdm "$_tgt" ) || return 1

	for file in modules.order modules.builtin; do
		if [ -f "$kerneldir"/$file ]; then
			cp "$kerneldir"/$file "$_tgt/lib/modules/$_kver/$file"
		fi
	done
	depmod $_kver -b "$_tgt"
}

# Usage: initfs_find_firmware <source> <kernel version> <features...>
mkinitfs_find_firmware() {
	local _src _tgt FW
	_src="${1%/}"
	_kver="${2}"
	shift 2

	mkinitfs_find_kmods "$_src" "$_kver" "$@" | xargs modinfo -F firmware | sort -u | while read FW; do
		[ -e "${_src}/lib/firmware/${FW}" ] && printf_n "${_src}/lib/firmware/${FW}"
	done
	return 0
}
# Usage: mkinitfs_initfs_firmware <source> <target> <kernel version> <features...>
mkinitfs_initfs_firmware() {
	local _src _tgt FW
	_src="${1%/}"
	_tgt="${2%/}"
	_kver="${3}"
	shift 3

	dir_is_readable "$_src/lib/firmware" || return 0

	rm -rf "$_tgt/lib/firmware"
	mkdir_is_writable "$_tgt/lib/firmware"

	mkinitfs_find_firmware "$_src" "$_kver" "$@" | while read FW; do
		[ -e "${FW}" ] && install -pD "${FW}" "${_tgt}${FW#${_src}}"
	done
	return 0
}

# Usage: mkinitfs_initfs_apk_keys <source> <target>
mkinitfs_initfs_apk_keys() {
	local _src _tgt
	_src="${1%/}"
	_tgt="${2%/}"

	mkdir_is_writable "$_tgt"/etc/apk/keys
	[ "$_hostkeys" ] && cp "/etc/apk/keys/"* "${_tgt}/etc/apk/keys/"
	cp "${_src}/etc/apk/keys/"* "${_tgt}/etc/apk/keys/"
}

# Usage: mkinitfs_cpio_sources <source>
mkinitfs_cpio_sources() {
	local _src
	_src="${1%/}"
	(cd "$_src" && find . | sort)
}

# Usage: mkinitfs_initfs_cpio <source> <output file>
mkinitfs_initfs_cpio() {
	local _src _out
	_src="${1%/}"
	_out="${2%/}"
	shift 2

	rm -f "$_out"
	umask 0022
	(cd "$_src" && find . | sort | cpio --quiet -o -H newc | gzip -9) > $_out
}

mkinitfs_wrapper_usage() {
	cat <<EOF
usage: mkinitfs [-nKklLh] [-b basedir] [-o outfile] [-t tempdir]
		[-c configfile] [-F features] [-P featuresdir]
		[-f fstab] [-i initfile] [kernelversion]
options:
	-b  prefix files and kernel modules with basedir
	-o  set another outfile
	-t  use tempdir when creating initramfs image
	-c  use configfile instead of default
	-F  use specified features
	-P  additional paths to find features
	-f  use fstab instead of default
	-i  use initfile as init instead of default
	-n  don't include kernel modules or firmware
	-K  copy also host keys to initramfs
	-k  keep tempdir
	X-q  Quiet mode
	-l  only list files that would have been used
	-L  list available features
	-h  print this help

EOF
	exit 1
}

mkinitfs() {
	local _opt _val _src _tgt _out _kver _features _conf _keeptmp _pd _d _listfiles _listfeatures
	_mkinitfs_nomods="no"
	while [ "${1}" != "${1#-}" ] ; do
		case "$1" in
			'-b' ) _src="${2%/}" ; shift 2 ; continue ;;
			'-o' ) _out="$2" ; shift 2 ; continue ;;
			'-t' ) _tgt="${2%/}" ; shift 2 ; continue ;;
			'-c' ) _conf="$2" ; shift 2 ; continue ;;
			'-F' ) _features="$2" ; shift 2 ; continue ;;
			'-P' ) _pd="${_pd:+ $_pd}$2" ; shift 2 ;; # Load features from files/dirs.
			'-f' ) initfs_src_fstab="$2" ; shift 2 ; continue ;;
			'-i' ) initfs_src_init="$2" ; shift 2 ; continue ;;
			'-n' ) _mkinitfs_nomods="yes" ; shift ;; # Modules included by default
			'-K' ) shift ; _hostkeys="yes";;
			'-k' ) _keeptmp="yes" ; shift ;; # Cleaning up tmp enabled by default
			'-l' ) shift ; _listfiles="yes" ;;
			'-L' ) shift ; _listfeatures="yes" ; exit 0 ;;
			'-h' ) shift ; mkinitfs_wrapper_usage ; exit 0 ;;
			'-q' ) shift ;;
			* ) shift ; mkinitfs_wrapper_usage ; exit 1 ;;

		esac
	done
	
	_kver="${1:-$(uname -r)}"
	shift

	[ "$_pd" ] && load_plugins "$_pd"

	_src="${_src:-/}"
	_tgt="${_tgt:-$(mktemp -dt mkinitfs.XXXXXX)}"
	_out="${_out:-${_kver#*-*-}}"


	if [ "$_conf" ] ; then
		file_is_readable "$_conf"
		local features
		. "$_conf"
		_features="${_features:+$_features }${features}"
		unset features
	fi



	if [ "$_listfeatures" = "yes" ] ; then printf_n $all_initfss ; exit 0 ; fi

	[ "$_features" ] || [ "$@" ] || ! warning "mkinitfs: Called with no features!" || return 1

	if [ "$_listfiles" = "yes" ] ; then 
		mkinitfs_find_files "$_src" "$_features"
		mkinitfs_find_kmods "$_src" "$_kver" "$_features"
		mkinitfs_find_firmware "$_src" "$_kver" "$_features"
		exit 0
	fi 

	dir_is_readable "$_src" || ! warning "mkinitfs: Could not read from source directory '$_src'"
	mkdir_is_writable "$_tgt" || ! warning "mkinitfs: Could create writable work directory '$_tgt'"

	mkdir_is_writable "${_out%/*?}" || ! warning "mkinitfs: Can not write to output directory '${_out%/*?}'"
	if [ -e "$_out" ] ; then
		rm -f "$_out"
		[ -e "$_out" ] && file_is_writable "${_out}" \
			|| ! warning "mkinitfs: Output file exists and can not be overwritten: '${_out}'"
	fi


	mkinitfs_main "$_src" "$_tgt" "$_out" "$_kver" $_features $@ || return 1
	
	[ "$_keeptmp" ] || rm -rf "$_tgt"

}


