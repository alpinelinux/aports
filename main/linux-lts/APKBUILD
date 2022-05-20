# Maintainer: Natanael Copa <ncopa@alpinelinux.org>

_flavor=lts
pkgname=linux-${_flavor}
pkgver=5.15.41
case $pkgver in
	*.*.*)	_kernver=${pkgver%.*};;
	*.*) _kernver=$pkgver;;
esac
pkgrel=0
pkgdesc="Linux lts kernel"
url="https://www.kernel.org"
depends="mkinitfs>=3.6.0"
_depends_dev="perl gmp-dev mpc1-dev mpfr-dev elfutils-dev bash flex bison zstd"
makedepends="$_depends_dev sed installkernel bc linux-headers linux-firmware-any openssl1.1-compat-dev mawk
	diffutils findutils zstd"
options="!strip"
_config=${config:-config-lts.${CARCH}}
install=
source="https://cdn.kernel.org/pub/linux/kernel/v${pkgver%%.*}.x/linux-$_kernver.tar.xz
	0001-powerpc-config-defang-gcc-check-for-stack-protector-.patch
	vmlinux-zstd.patch

	lts.aarch64.config
	lts.armv7.config
	lts.x86.config
	lts.x86_64.config
	lts.ppc64le.config
	lts.s390x.config

	virt.aarch64.config
	virt.armv7.config
	virt.ppc64le.config
	virt.x86.config
	virt.x86_64.config
	"
subpackages="$pkgname-dev:_dev:$CBUILD_ARCH"
_flavors=
for _i in $source; do
	case $_i in
	*.$CARCH.config)
		_f=${_i%.$CARCH.config}
		_flavors="$_flavors ${_f}"
		if [ "linux-$_f" != "$pkgname" ]; then
			subpackages="$subpackages linux-${_f}::$CBUILD_ARCH linux-${_f}-dev:_dev:$CBUILD_ARCH"
		fi
		;;
	esac
done

if [ "${pkgver%.0}" = "$pkgver" ]; then
	source="$source
	https://cdn.kernel.org/pub/linux/kernel/v${pkgver%%.*}.x/patch-$pkgver.xz"
fi
arch="all !armhf !riscv64"
license="GPL-2.0"

# secfixes:
#   5.10.4-r0:
#     - CVE-2020-29568
#     - CVE-2020-29569

prepare() {
	local _patch_failed=
	cd "$srcdir"/linux-$_kernver
	if [ "$_kernver" != "$pkgver" ]; then
		msg "Applying patch-$pkgver.xz"
		unxz -c < "$srcdir"/patch-$pkgver.xz | patch -p1 -N
	fi

	# first apply patches in specified order
	for i in $source; do
		case $i in
		*.patch)
			msg "Applying $i..."
			if ! patch -s -p1 -N -i "$srcdir"/$i; then
				echo $i >>failed
				_patch_failed=1
			fi
			;;
		esac
	done

	if ! [ -z "$_patch_failed" ]; then
		error "The following patches failed:"
		cat failed
		return 1
	fi

	# remove localversion from patch if any
	rm -f localversion*
}

_kernelarch() {
	local arch="$1"
	case "$arch" in
		aarch64*) arch="arm64" ;;
		arm*) arch="arm" ;;
		mips*) arch="mips" ;;
		ppc*) arch="powerpc" ;;
		s390*) arch="s390" ;;
	esac
	echo "$arch"
}

_prepareconfig() {
	local _flavor="$1"
	local _arch="$2"
	local _config=$_flavor.$_arch.config
	local _builddir="$srcdir"/build-$_flavor.$_arch
	mkdir -p "$_builddir"
	echo "-$pkgrel-$_flavor" > "$_builddir"/localversion-alpine \
		|| return 1

	cp "$srcdir"/$_config "$_builddir"/.config
	msg "Configuring $_flavor kernel ($_arch)"
	make -C "$srcdir"/linux-$_kernver \
		O="$_builddir" \
		ARCH="$(_kernelarch $_arch)" \
		olddefconfig
}

listconfigs() {
	for i in $source; do
		case "$i" in
			*.config) echo $i;;
		esac
	done
}

prepareconfigs() {
	for _config in $(listconfigs); do
		local _flavor=${_config%%.*}
		local _arch=${_config%.config}
		_arch=${_arch#*.}
		local _builddir="$srcdir"/build-$_flavor.$_arch
		_prepareconfig "$_flavor" "$_arch"
	done
}

# this is supposed to be run before version is bumped so we can compare
# what new kernel config knobs are introduced
prepareupdate() {
	clean && fetch && unpack && prepare && deps
	prepareconfigs
	rm -r "$srcdir"/linux-$_kernver
}

updateconfigs() {
	if ! [ -d "$srcdir"/linux-$_kernver ]; then
		deps && fetch && unpack && prepare
	fi
	for _config in $(listconfigs); do
		local _flavor=${_config%%.*}
		local _arch=${_config%.config}
		_arch=${_arch#*.}
		local _builddir="$srcdir"/build-$_flavor.$_arch
		mkdir -p "$_builddir"
		echo "-$pkgrel-$_flavor" > "$_builddir"/localversion-alpine
		local actions="listnewconfig oldconfig"
		if ! [ -f "$_builddir"/.config ]; then
			cp "$srcdir"/$_config "$_builddir"/.config
			actions="olddefconfig"
			env | grep ^CONFIG_ >> "$_builddir"/.config || true
		fi
		make -j1 -C "$srcdir"/linux-$_kernver \
			O="$_builddir" \
			ARCH="$(_kernelarch $_arch)" \
			$actions savedefconfig

		cp "$_builddir"/defconfig "$startdir"/$_config
	done
}

build() {
	unset LDFLAGS
	export KBUILD_BUILD_TIMESTAMP="$(date -Ru${SOURCE_DATE_EPOCH:+d @$SOURCE_DATE_EPOCH})"
	for i in $_flavors; do
		_prepareconfig "$i" "$CARCH"
	done
	for i in $_flavors; do
		msg "Building $i kernel"
		cd "$srcdir"/build-$i.$CARCH
		make ARCH="$(_kernelarch $CARCH)" \
			CC="${CC:-gcc}" \
			AWK="${AWK:-mawk}" \
			KBUILD_BUILD_VERSION="$((pkgrel + 1 ))-Alpine"
	done
}

_package() {
	local _buildflavor="$1" _outdir="$2"
	local _abi_release=${pkgver}-${pkgrel}-${_buildflavor}
	export KBUILD_BUILD_TIMESTAMP="$(date -Ru${SOURCE_DATE_EPOCH:+d @$SOURCE_DATE_EPOCH})"

	cd "$srcdir"/build-$_buildflavor.$CARCH
	# modules_install seems to regenerate a defect Modules.symvers on s390x. Work
	# around it by backing it up and restore it after modules_install
	cp Module.symvers Module.symvers.backup

	mkdir -p "$_outdir"/boot "$_outdir"/lib/modules

	local _install
	case "$CARCH" in
		arm*|aarch64) _install="zinstall dtbs_install";;
		*) _install=install;;
	esac

	make -j1 modules_install $_install \
		ARCH="$(_kernelarch $CARCH)" \
		INSTALL_MOD_PATH="$_outdir" \
		INSTALL_PATH="$_outdir"/boot \
		INSTALL_DTBS_PATH="$_outdir/boot/dtbs-$_buildflavor"

	cp Module.symvers.backup Module.symvers

	rm -f "$_outdir"/lib/modules/${_abi_release}/build \
		"$_outdir"/lib/modules/${_abi_release}/source
	rm -rf "$_outdir"/lib/firmware

	install -D -m644 include/config/kernel.release \
		"$_outdir"/usr/share/kernel/$_buildflavor/kernel.release
}

# main flavor installs in $pkgdir
package() {
	depends="$depends linux-firmware-any"

	_package lts "$pkgdir"
}

# subflavors install in $subpkgdir
virt() {
	_package virt "$subpkgdir"
}

_dev() {
	local _flavor=$(echo $subpkgname | sed -E 's/(^linux-|-dev$)//g')
	local _abi_release=${pkgver}-${pkgrel}-$_flavor
	# copy the only the parts that we really need for build 3rd party
	# kernel modules and install those as /usr/src/linux-headers,
	# simlar to what ubuntu does
	#
	# this way you dont need to install the 300-400 kernel sources to
	# build a tiny kernel module
	#
	pkgdesc="Headers and script for third party modules for $_flavor kernel"
	depends="$_depends_dev"
	local dir="$subpkgdir"/usr/src/linux-headers-${_abi_release}
	export KBUILD_BUILD_TIMESTAMP="$(date -Ru${SOURCE_DATE_EPOCH:+d @$SOURCE_DATE_EPOCH})"

	# first we import config, run prepare to set up for building
	# external modules, and create the scripts
	mkdir -p "$dir"
	local _builddir="$srcdir"/build-$_flavor.$CARCH
	cp -a "$_builddir"/.config "$_builddir"/localversion-alpine \
		"$dir"/

	make -j1 -C "$srcdir"/linux-$_kernver \
		O="$dir" \
		ARCH="$(_kernelarch $CARCH)" \
		prepare modules_prepare scripts

	# remove the stuff that points to real sources. we want 3rd party
	# modules to believe this is the soruces
	rm "$dir"/Makefile "$dir"/source

	# copy the needed stuff from real sources
	#
	# this is taken from ubuntu kernel build script
	# http://kernel.ubuntu.com/git/ubuntu/ubuntu-zesty.git/tree/debian/rules.d/3-binary-indep.mk
	cd "$srcdir"/linux-$_kernver
	find .  -path './include/*' -prune \
		-o -path './scripts/*' -prune -o -type f \
		\( -name 'Makefile*' -o -name 'Kconfig*' -o -name 'Kbuild*' -o \
		   -name '*.sh' -o -name '*.pl' -o -name '*.lds' -o -name 'Platform' \) \
		-print | cpio -pdm "$dir"

	cp -a scripts include "$dir"

	find $(find arch -name include -type d -print) -type f \
		| cpio -pdm "$dir"

	install -Dm644 "$srcdir"/build-$_flavor.$CARCH/Module.symvers \
		"$dir"/Module.symvers

	mkdir -p "$subpkgdir"/lib/modules/${_abi_release}
	ln -sf /usr/src/linux-headers-${_abi_release} \
		"$subpkgdir"/lib/modules/${_abi_release}/build
}

sha512sums="
d25ad40b5bcd6a4c6042fd0fd84e196e7a58024734c3e9a484fd0d5d54a0c1d87db8a3c784eff55e43b6f021709dc685eb0efa18d2aec327e4f88a79f405705a  linux-5.15.tar.xz
214c54a839ae37849715520f4b1049f0df5366ca32522701b43afecfad116794c4542940ba32d389f28f2549d08c03d148f884cb8e565b75aa3c0cad6a4887b7  0001-powerpc-config-defang-gcc-check-for-stack-protector-.patch
d26d3f99fdcbd0f56e9af32a281870bbfd9fe6a12d17921ef3876e72bd1e92a3c131e06567078a45c11a41826b39d3068cc6f0e89f67d9e16a14825984869268  vmlinux-zstd.patch
d0259edbe3a3ee6bbaa170b85b505a93a5593c639ef180dd960d3ed599165680446c137c727a74fe9069cc8007413580bbe5d25c2f52b061b622e53beb52e6d7  lts.aarch64.config
723fd8d1f64ebdcd197abc9caa55a367626ee6b71ec0313beef2fab2738ff7edc7ce9088e73044ca87f467df5b7e3c84f402ef9f1df3cba99499836e16f151d8  lts.armv7.config
292061d4afca8f9a513315fd493978c595fc1e67a09d86485e781295d0974ad19581c06300ebe7819091845f34ab620c79f45ebee7a82fefdbc7d7b4e676c67a  lts.x86.config
1cf94d8e40396d607faab66e3a528a61a95b0543d091a7fa34bc20a9aa6daacfc57d5696271b04bee652cb93a028a342e58f6952209cc7c19762b219599849b5  lts.x86_64.config
5a2f88061dfe0c6cdda5b78614e12ad403b6d010d8cf9703b795c12e445bf15502e85144f6b881f6682a9527cd68ab610651b4e9448ece3f992b224bc3e52b85  lts.ppc64le.config
4e3a21d1fa3624f6d572496383e13c2bc122183cba2265ec6becf72adb376adb37d5c42d4dea6cadee02475af7af1fa54097088aac90576728c92bb6ed1fcaaa  lts.s390x.config
98cc79bc6cfbd85bed7663267053d70054d26320c3936597f853d58b9780995666038e6030efc09023e46a8032e446150806fd24e260499839f0401d94504520  virt.aarch64.config
35d052d34fd4552f67f7a8250577bd82f85e43683ff4567cd2c275c3db8975c9c5643ec27fcbc56e03b14bd9ec813093029854f386c8f8a218e56b8ee23b9201  virt.armv7.config
11048d5a96ac8013e9bd6c8828a028ef7d8f6ac60bb1403ffe8fcb19c4008e9cad0f6b9658d59f25d9081b5cb06656f2e564b35ec493bb61774eb78cb9a05d6a  virt.ppc64le.config
c0ddfee06f958192653d840ca14fd60d5d3d88311b1335d691fc4d4c5a7575c5771351a0ec5e3d34544c76c34e4ee8731cc45fa18da77909924f0b9b6ef75d87  virt.x86.config
3ffe449f4d6e54a76f331530ba3a31aa1686ae07fa837a33a9d1f76c774a9cdae052a8ca669042b35b8f39e7c46957b4c23981ec211340427aa18ac4c65bb060  virt.x86_64.config
1c7ece228174f5c3049ec44cafa588117d6c470567a95ba4c926fa370b17b8a90256c40df657fc923ec32303268abeeb6249943eee69cad73be38156628aab6f  patch-5.15.41.xz
"
