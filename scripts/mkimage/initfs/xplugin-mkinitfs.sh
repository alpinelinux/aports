
mkinitfs_feature_files() {
	local _src="${1%/}"
	local suffix="$2"
	shift 2

	local glob file
	for f in $@; do
		[ "$(type -t _initfs_${f}_${suffix})" ] \
			|| ! warning "Could not get $suffix list for feature '$f'!" \
			|| continue

		for glob in $( _initfs_${f}_${suffix} | sed -e '/^$/d' -e '/^#/d' -e "s|^/*|$_src/|") ; do
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
		$(feature_files "$_src" files "$@") \
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
	for file in $(mkinitfs_feature_files "${_src}" modules "$@"); do
		echo ${file#${_src%/}/}
	done | mkinitfs_find_kmod_deps "$_src" "$_kver"
	cd "$oldpwd"
}

mkinitfs_kmods() {
	local _src _tgt
	_src="${1%/}"
	_tgt="${2%/}"
	_kver="${3}"
	shift 3

	local file=
	local kerneldir="$_src/lib/modules/$_kver"

	rm -rf "$_tgt"/lib/modules
	# make sure we have modules.dep
	if ! [ -f "$kerneldir"/modules.dep ]; then
		depmod -b "$_src" $kernel
	fi

	local oldpwd="$PWD"
	cd "${_src}"
	for file in $(mkinitfs_find_kmods "$_src" "$_kver" "$@" ); do
		echo "${file#/}"
	done | sort -u | cpio --quiet -pdm "$_tgt" || return 1
	for file in modules.order modules.builtin; do
		if [ -f "$kerneldir"/$file ]; then
			cp "$kerneldir"/$file "$_tgt"/lib/modules/$_kver/
		fi
	done
	depmod $_kver -b "$_tgt"
	cd "$oldpwd"
}

# Usage: initfs_copy_firmware_deps <source> <target>
mkinitfs_firmware() {
	local _src _tgt
	_src="${1%/}"
	_tgt="${2%/}"

	rm -rf "$_tgt"/lib/firmware
	mkdir_is_writable "$_tgt"/lib/firmware

	find "$_tgt"/lib/modules -type f -name "*.ko" | xargs modinfo -F firmware | sort -u | while read FW; do
		[ -e "${_src}/lib/firmware/${FW}" ] && install -pD "${_src}/lib/firmware/${FW}" "$_tgt"/lib/firmware/$FW
	done
	return 0
}

mkinitfs_apk_keys() {
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

mkinitfs_cpio() {
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
	-c  use configfile instead of $config
	-f  use fstab instead of $fstab
	-F  use specified features
	-h  print this help
	-i  use initfile as init instead of $init
	-k  keep tempdir
	-K  copy also host keys to initramfs
	-l  only list files that would have been used
	-L  list available features
	-n  don't include kernel modules or firmware
	-o  set another outfile
	-P  prepend features.d search path
	-q  Quiet mode
	-t  use tempdir when creating initramfs image

EOF
	exit 1
}

# Usage: mkinitfs <source dir> <temp dir> <outfile> <kernel version> <initfs-features...>
mkinitfs() {
	local _src _tgt
	_src="${1%/}"
	_tgt="${2%/}"
	_out="${3%/}"
	_kver="${4}"
	shift 4


	initfs_src_fstab="${initfs_src_fstab}"
	initfs_src_init="${initfs_src_init}"
	initfs_src_passwd="${initfs_src_passwd}"
	initfs_src_group="${initfs_src_group}"

	mkinitfs_base "$_src" "$_tgt" "$@" \
		&& mkinitfs_kmods "$_src" "$_tgt" "$_kver" "$@" \
		&& mkinitfs_firmware "$_src" "$_tgt" \
		&& mkinitfs_apk_keys "$_src" "$_tgt" \
		&& mkinitfs_cpio "$_tgt" "$_out" \
		|| ! warning "mkintfs failed!" || return 1
}
