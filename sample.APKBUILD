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
arch="all"
license="GPL"
depends=
depends_dev=
makedepends="$depends_dev"
install=
subpackages="$pkgname-dev $pkgname-doc"
source="http://downloads.sourceforge.net/$pkgname/$pkgname-$pkgver.tar.gz"


_builddir="$srcdir"/$pkgname-$pkgver

prepare() {
	cd "$_builddir"
	# apply patches here
}

build() {
	cd "$_builddir"
	./configure --prefix=/usr \
		--sysconfdir=/etc \
		--mandir=/usr/share/man \
		--infodir=/usr/share/info
	make || return 1
}

package() {
	cd "$_builddir"
	make DESTDIR="$pkgdir" install

	# remove the 2 lines below (and this) if there is no init.d script
	# install -m755 -D "$srcdir"/$pkgname.initd "$pkgdir"/etc/init.d/$pkgname
	# install -m644 -D "$srcdir"/$pkgname.confd "$pkgdir"/etc/conf.d/$pkgname
}

md5sums="" #generate with 'abuild checksum'
