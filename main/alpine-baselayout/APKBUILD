# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-baselayout
pkgver=2.0_rc1
pkgrel=1
pkgdesc="Alpine base dir structure and init scripts"
url=http://git.alpinelinux.org/cgit/alpine-baselayout
depends=
source="http://git.alpinelinux.org/cgit/$pkgname/snapshot/$pkgname-$pkgver.tar.bz2
	"
arch="x86 x86_64"
license=GPL-2

build() {
	cd "$srcdir"/$pkgname-$pkgver
	rm -f src/mkmntdirs
	make
}

package() {
	cd "$srcdir"/$pkgname-$pkgver
	make install PREFIX= DESTDIR="$pkgdir" || return 1
}
md5sums="76d61057c9e21d8e3ef85933a20b814d  alpine-baselayout-2.0_rc1.tar.bz2"
