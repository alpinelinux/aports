# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-baselayout
pkgver=2.2.0
pkgrel=6
pkgdesc="Alpine base dir structure and init scripts"
url=http://git.alpinelinux.org/cgit/alpine-baselayout
depends=
options="!fhs"
install="$pkgname.pre-upgrade $pkgname.post-upgrade"
source="http://git.alpinelinux.org/cgit/$pkgname.git/snapshot/$pkgname-$pkgver.tar.bz2
	0001-mount-media-usb-as-ro-by-default.patch
	0001-profile-set-LANG-en.utf8.patch
	"
arch="all"
license=GPL-2

_builddir="$srcdir"/$pkgname-$pkgver
prepare() {
	cd "$_builddir"
	for i in $source; do
		case $i in
		*.patch) msg $i; patch -p1 -i "$srcdir"/$i || return 1;;
		esac
	done
}

build() {
	cd "$_builddir"
	rm -f src/mkmntdirs
	make
}

package() {
	cd "$_builddir"
	make install PREFIX= DESTDIR="$pkgdir" || return 1
	rm -rf "$pkgdir"/etc/issue "$pkgdir"/usr/share/udhcpc \
		"$pkgdir"/etc/init.d/vlan
	
	# lets blacklist microcode
	echo "blacklist microcode" > "$pkgdir"/etc/modprobe.d/microcode.conf
}
md5sums="225dbf5b50ae6d3d188ff92c5746bc36  alpine-baselayout-2.2.0.tar.bz2
0619f806d91a5293a95debe9fb0124b5  0001-mount-media-usb-as-ro-by-default.patch
d53f5b40ed84866fde4a14906ff6658c  0001-profile-set-LANG-en.utf8.patch"
sha256sums="aaa124e12c70c63772f374b0e041a5a1e8db2776cedc453bd04626edfacd22aa  alpine-baselayout-2.2.0.tar.bz2
da88b6415da532aa89bbfadcb671d8dbba65ceb232ee49aee179062c15320574  0001-mount-media-usb-as-ro-by-default.patch
b94791e6ec326816859c20d60543c30e360a7ba27c8ea45440fab5b0091cd830  0001-profile-set-LANG-en.utf8.patch"
sha512sums="9a327c15b779d794623c8e4cecc82f0a014d15a8ffeb1de6eadf8ccde1e65593d2d14665bd95c6d9da4e15fe9db2f8342af7bdf834af7378007f8eaecbb7d6b4  alpine-baselayout-2.2.0.tar.bz2
41c4d671d3263d1b08a04021a2da7aec867807ba05afb3926eb6953a609fbf6e0728bf8f75a08853a2258a763beb6845f80d7ee97256cfa1792e6cf57557615e  0001-mount-media-usb-as-ro-by-default.patch
d09330579eaabf698b05ea36b13bd21dba2d7ff969fb11ab7549b6ed2e2e331525aa171d5f4a7e9bf4cd5b7883d004d8c23bdc0da8c59b0993a0bb69ec497014  0001-profile-set-LANG-en.utf8.patch"
