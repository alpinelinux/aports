# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-baselayout
pkgver=2.0_beta2
pkgrel=0
pkgdesc="Alpine base dir structure and init scripts"
url=http://git.alpinelinux.org/cgit/alpine-baselayout
depends=
source="http://git.alpinelinux.org/cgit/$pkgname/snapshot/$pkgname-$pkgver.tar.bz2
	"
license=GPL-2

build() {
	cd "$srcdir"/$pkgname-$pkgver
	make
	make install PREFIX= DESTDIR="$pkgdir" || return 1
}
md5sums="962bcd31be3ce2db90e997ddc9f612fc  alpine-baselayout-2.0_beta2.tar.bz2"
