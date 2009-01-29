# This is an example APKBUILD file. Use this as a start to creating your own,
# and remove these comments. 
# NOTE: Please fill out the license field for your package! If it is unknown,
# then please put 'unknown'.

# Contributor: Your Name <youremail@domain.com>
# Maintainer: Your Name <youremail@domain.com>
pkgname=NAME
pkgver=VERSION
pkgrel=0
pkgdesc=""
url=""
license="GPL"
depends=""
makedepends=""
install=
subpackages="$pkgname-doc $pkgname-dev"
source="http://downloads.sourceforge.net/$pkgname/$pkgname-$pkgver.tar.gz"

build() {
	cd "$srcdir/$pkgname-$pkgver"

	./configure --prefix=/usr \
		--sysconfdir=/etc \
		--mandir=/usr/share/man \
		--infodir=/usr/share/info
	make || return 1
	make DESTDIR="$pkgdir" install

	# install -m755 -D "$srcdir"/$pkgname.initd "$pkgdir"/etc/init.d/$pkgname
	# install -m644 -D "$srcdir"/$pkgname.confd "$pkgdir"/etc/conf.d/$pkgname
}

md5sums="" #generate with 'abuild checksum'
