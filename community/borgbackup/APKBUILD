# Contributor: Olivier Mauras <olivier@mauras.ch>
# Maintainer: Jakub Jirutka <jakub@jirutka.cz>
pkgname=borgbackup
pkgver=1.1.3
pkgrel=0
pkgdesc="Deduplicating backup program"
url="https://borgbackup.readthedocs.io/"
arch="all"
license="BSD"
depends="python3 py3-msgpack py3-zmq"
makedepends="python3-dev lz4-dev acl-dev attr-dev libressl-dev linux-headers"
source="https://github.com/$pkgname/borg/releases/download/$pkgver/$pkgname-$pkgver.tar.gz"
builddir="$srcdir/$pkgname-$pkgver"

build() {
	cd "$builddir"
	python3 setup.py build
}

package() {
	cd "$builddir"
	python3 setup.py install --prefix=/usr --root="$pkgdir"

	# Clean some useless files.
	cd "$pkgdir"/usr/lib/python*/site-packages/borg
	find . -name '*.h' -delete -o -name '*.c' -delete -o -name '*.pyx' -delete
}

sha512sums="8378e4f805bfb3e9a4e454f5ccfa58eef0517f13a2e8a2c3c6cbdb0304b763fa67152963b17d677daff09590eb777f12fbe1f3f69c3459bcc68781e5a747cb49  borgbackup-1.1.3.tar.gz"
