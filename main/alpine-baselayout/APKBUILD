# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-baselayout
pkgver=2.1.1
pkgrel=3
pkgdesc="Alpine base dir structure and init scripts"
url=http://git.alpinelinux.org/cgit/alpine-baselayout
depends=
install="$pkgname.pre-upgrade"
source="http://git.alpinelinux.org/cgit/$pkgname.git/snapshot/$pkgname-$pkgver.tar.bz2
	0001-blacklist-viafb-and-e_powersaver.patch
	"
arch="all"
license=GPL-2

_builddir="$srcdir"/$pkgname-$pkgver
prepare() {
	cd "$_builddir"
}

build() {
	cd "$_builddir"
	rm -f src/mkmntdirs
	make
}

package() {
	cd "$_builddir"
	make install PREFIX= DESTDIR="$pkgdir" || return 1
	mkdir "$pkgdir"/run
	rm -rf "$pkgdir"/etc/issue "$pkgdir"/usr/share/udhcpc
}
md5sums="4f47c32a0e88ae0bd4673a07478525c0  alpine-baselayout-2.1.1.tar.bz2
3e219db536b13811c34e03c9c32229cb  0001-blacklist-viafb-and-e_powersaver.patch"
