# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-baselayout
pkgver=2.0_beta1
pkgrel=1
pkgdesc="Alpine base dir structure and init scripts"
url=http://git.alpinelinux.org/cgit/alpine-baselayout
depends=
source="http://git.alpinelinux.org/cgit/$pkgname/snapshot/$pkgname-$pkgver.tar.bz2
	0001-profile-change-default-path.patch
	"
license=GPL-2

build() {
	cd "$srcdir"/$pkgname-$pkgver
	patch -p1 < ../0001-profile-change-default-path.patch || return 1
	make
	make install PREFIX= DESTDIR="$pkgdir" || return 1
}
md5sums="6b25fc0c261e9182a68582c38249a3e8  alpine-baselayout-2.0_beta1.tar.bz2
085c7e50bb57307fd9a24ee8c14e4749  0001-profile-change-default-path.patch"
