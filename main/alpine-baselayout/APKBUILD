# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-baselayout
pkgver=2.0_rc1
pkgrel=3
pkgdesc="Alpine base dir structure and init scripts"
url=http://git.alpinelinux.org/cgit/alpine-baselayout
depends=
source="http://git.alpinelinux.org/cgit/$pkgname.git/snapshot/$pkgname-$pkgver.tar.bz2
	0001-profile-set-system-environment-charset-to-UTF-8.patch
	"
arch="all"
license=GPL-2

prepare() {
	cd "$srcdir"/$pkgname-$pkgver
	patch -p1 < "$srcdir"/0001-profile-set-system-environment-charset-to-UTF-8.patch
}

build() {
	cd "$srcdir"/$pkgname-$pkgver
	rm -f src/mkmntdirs
	make
}

package() {
	cd "$srcdir"/$pkgname-$pkgver
	make install PREFIX= DESTDIR="$pkgdir" || return 1
}
md5sums="76d61057c9e21d8e3ef85933a20b814d  alpine-baselayout-2.0_rc1.tar.bz2
b4eb3fdb4ca0aa136c87ca0c32e25742  0001-profile-set-system-environment-charset-to-UTF-8.patch"
