# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-baselayout
pkgver=2.2.0
pkgrel=5
pkgdesc="Alpine base dir structure and init scripts"
url=http://git.alpinelinux.org/cgit/alpine-baselayout
depends=
options="!fhs"
install="$pkgname.pre-upgrade $pkgname.post-upgrade"
source="http://git.alpinelinux.org/cgit/$pkgname.git/snapshot/$pkgname-$pkgver.tar.bz2
	0001-mount-media-usb-as-ro-by-default.patch
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
0619f806d91a5293a95debe9fb0124b5  0001-mount-media-usb-as-ro-by-default.patch"
