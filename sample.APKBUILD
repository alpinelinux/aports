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
source="https://downloads.sourceforge.net/$pkgname/$pkgname-$pkgver.tar.gz"

builddir="$srcdir"/$pkgname-$pkgver

prepare() {
	cd "$builddir"
}

build() {
	cd "$builddir"
	./configure --prefix=/usr \
		--sysconfdir=/etc \
		--mandir=/usr/share/man \
		--infodir=/usr/share/info
	make
}

package() {
	cd "$builddir"
	make DESTDIR="$pkgdir" install

	# remove the 2 lines below (and this) if there is no init.d script
	# install -m755 -D "$srcdir"/$pkgname.initd "$pkgdir"/etc/init.d/$pkgname
	# install -m644 -D "$srcdir"/$pkgname.confd "$pkgdir"/etc/conf.d/$pkgname
}

check() {
	# uncomment the 2 lines below if there is a testsuite.  we assume the testsuite
	# is run using "make check", which is the default for autotools-based build systems.
	# cd "$builddir"
	# make check
}

md5sums="" #generate with 'abuild checksum'
