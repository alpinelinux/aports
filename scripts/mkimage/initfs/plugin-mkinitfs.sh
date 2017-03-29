

# Usage: mkinitfs <source dir> <temp dir> <outfile> <kernel version> <initfs-features...>
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
			&& mkinitfs_initfs_firmware "$_src" "$_tgt" ; \
		fi \
		&& mkinitfs_initfs_apk_keys "$_src" "$_tgt" \
		&& mkinitfs_initfs_cpio "$_tgt" "$_out" \
		&& _err=0

	[ $_err -ne 0 ] && warning "mkintfs: Building initfs failed!"
	return $_err
}

mkinitfs_feature_files() {
	local _src="${1%/}"
	local suffix="$2"
	shift 2

	local f glob file fname
	for f in $@; do
		fname="$(type "_initfs_${f//-/_}_${suffix}")"
		[ "$fname" != "${fname//shell function/}" ] \
			|| ! warning "mkinitfs: Could not get $suffix list for feature '$f'!" \
			|| continue

		for glob in $( _initfs_${f//-/_}_${suffix} | sed -e '/^$/d' -e '/^#/d' -e "s|^/*|$_src/|") ; do
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


	( cd $_src && lddtree -R "$_src" -l --no-auto-root \
		$(mkinitfs_feature_files "$_src" files "$@") \
		| sed -e "s|^$_src||" | sort -u \
		| cpio --quiet -pdm "$_tgt"
	) || return 1
	
	# copy init
	install -m755 "$initfs_src_init" "$_tgt"/init || return 1
	for i in "$initfs_src_fstab" "$initfs_src_passwd" "$initfs_src_group"; do
		install -Dm644 "$i" "$_tgt"/etc/${i##*/} || return 1
	done
}

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

mkinitfs_find_kmods() {
	local _src _tgt
	_src="${1%/}"
	_kver="${2%/}"
	shift 2

	local oldpwd="$PWD"
	cd "$_src" || return 1
	for file in $(mkinitfs_feature_files "${_src}" modules "$@") ; do
		echo ${file#${_src%/}/}
	done | mkinitfs_find_kmod_deps "$_src" "$_kver"
	cd "$oldpwd"
}

mkinitfs_initfs_kmods() {
	local _src _tgt
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

	local oldpwd="$PWD"
	cd "${_src}"
	for file in $(mkinitfs_find_kmods "$kerneldir" "$_kver" "$@" ); do
		echo "${file#/}"
	done | sort -u | cpio --quiet -pdm "$_tgt" || return 1

	for file in modules.order modules.builtin; do
		if [ -f "$kerneldir"/$file ]; then
			cp "$kerneldir"/$file "$_tgt/lib/modules/$_kver/$file"
		fi
	done
	depmod $_kver -b "$_tgt"
	cd "$oldpwd"
}

# Usage: initfs_copy_firmware_deps <source> <target>
mkinitfs_initfs_firmware() {
	local _src _tgt
	_src="${1%/}"
	_tgt="${2%/}"

	rm -rf "$_tgt/lib/firmware"
	mkdir_is_writable "$_tgt/lib/firmware"

	find "$_tgt/lib/modules" -type f -name "*.ko" | xargs modinfo -F firmware | sort -u | while read FW; do
		[ -e "${_src}/lib/firmware/${FW}" ] && install -pD "${_src}/lib/firmware/${FW}" "$_tgt"/lib/firmware/$FW
	done
	return 0
}

mkinitfs_initfs_apk_keys() {
	local _src _tgt
	_src="${1%/}"
	_tgt="${2%/}"

	mkdir_is_writable "$_tgt"/etc/apk/keys
	[ "$_hostkeys" ] && cp "/etc/apk/keys/"* "$_tgt"/etc/apk/keys/
	cp "${_src}/etc/apk/keys/"* "$_tgt"/etc/apk/keys/
}

mkinitfs_cpio_sources() {
	local _src _tgt
	_src="${1%/}"
	(cd "$_src" && find . | sort)
}

mkinitfs_initfs_cpio() {
	local _src _tgt
	_src="${1%/}"
	_out="${2%/}"
	shift 2

	rm -f "$_out"
	umask 0022
	(cd "$_src" && find . | sort | cpio --quiet -o -H newc | gzip -9) > $_out
}

mkinitfs_old_usage() {
	cat <<EOF
usage: mkinitfs [-hkKLln] [-b basedir] [-c configfile] [-F features] [-f fstab]
		[-i initfile ] [-o outfile] [-P featuresdir] [-t tempdir] [kernelversion]"
options:
	-b  prefix files and kernel modules with basedir
	-t  use tempdir when creating initramfs image
	-o  set another outfile
	-c  use configfile instead of default
	-F  use specified features
	-f  use fstab instead of default
	-i  use initfile as init instead of default
	-k  keep tempdir
	-h  print this help
	X-K  copy also host keys to initramfs
	X-l  only list files that would have been used
	X-L  list available features
	X-n  don't include kernel modules or firmware
	X-P  prepend features.d search path
	X-q  Quiet mode

EOF
	exit 1
}

mkinitfs() {
	local _opt _val _src _tgt _out _kver _features _conf _keeptmp
	_mkinitfs_nomods=""
	while [ "${1}" != "${1#-}" ] ; do
		case "$1" in
			'-b' ) _src="${2%/}" ; shift 2 ; continue ;;
			'-t' ) _tgt="${2%/}" ; shift 2 ; continue ;;
			'-o' ) _out="$2" ; shift 2 ; continue ;;
			'-c' ) _conf="$2" ; shift 2 ; continue ;;
			'-F' ) _features="$2" ; shift 2 ; continue ;;
			'-f' ) initfs_src_fstab="$2" ; shift 2 ; continue ;;
			'-i' ) initfs_src_init="$2" ; shift 2 ; continue ;;
			'-k' ) _keeptmp="yes" ; shift ;; # Enabled by default
			'-n' ) _mkinitfs_nomods="yes" ; shift ;; # Enabled by default
			'-K' ) shift ;; # Enabled by default
			'-P' ) shift 2 ;; # features.d no longer used
			'-l' ) shift ; ! warning "mkinitfs: '-l' Not currently reimplemented!" ;;
			'-L' ) shift ; ! warning "mkinitfs: '-L' Not yet reimplemented!" ;;
			'-h' ) shift ; usage ; exit 0 ;;
			* ) shift ; usage ; exit 1 ;;

		esac
	done
	
	_kver="${1:-$(uname -r)}"
	shift
	

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

	[ "$_features" ] || [ "$@" ] || ! warning "mkinitfs: Called with no features!"

	dir_is_readable "$_src" || ! warning "mkinitfs: Could not read from source directory '$_src'"
	mkdir_is_writable "$_tgt" || ! warning "mkinitfs: Could create writable work directory '$_tgt'"

	mkdir_is_writable "${_out%/*?}" || ! warning "mkinitfs: Can not write to output directory '${_out%/*?}'"
	if [ -e "$_out" ] ; then
		rm -f "$_out"
		[ -e "$_out" ] && file_is_writable "${_out}" \
			|| ! warning "mkinitfs: Output file exists and can not be overwritten: '${_out}'"
	fi


	mkinitfs_main "$_src" "$_tgt" "$_out" "$_kver" $_features $@ || return 1
	
	#[ "$_keeptmp" ] || rm -rf "$_tgt"

}


