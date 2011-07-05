# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-baselayout
pkgver=2.1.1
pkgrel=0
pkgdesc="Alpine base dir structure and init scripts"
url=http://git.alpinelinux.org/cgit/alpine-baselayout
depends=
install="$pkgname.pre-upgrade"
source="http://git.alpinelinux.org/cgit/$pkgname.git/snapshot/$pkgname-$pkgver.tar.bz2
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
	rm -f "$pkgdir"/etc/issue
}
md5sums="4f47c32a0e88ae0bd4673a07478525c0  alpine-baselayout-2.1.1.tar.bz2"
