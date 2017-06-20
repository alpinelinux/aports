# Contributor: Mihnea Saracin <mihnea.saracin@rinftech.com>
# Maintainer: Mihnea Saracin <mihnea.saracin@rinftech.com>
pkgname=pypy3
pkgver=5.8.0
pkgrel=0
pkgdesc="A fast, compliant alternative implementation of the Python 3 language"
url="https://github.com/HorridBro/AlpinePyPy3"
arch="all"
license="MIT"
depends=""
makedepends="python2 linux-headers libffi-dev pkgconf zlib-dev bzip2-dev 
             sqlite-dev ncurses-dev readline-dev libressl-dev expat-dev 
             gdbm-dev paxmark tk tk-dev py3-cffi py2-cffi xz xz-dev"
subpackages="$pkgname-dev"
source="https://bitbucket.org/pypy/pypy/downloads/$pkgname-v$pkgver-src.tar.bz2
        conststdio.patch
        dbremove.patch
        libpthread.patch
        nomandelbrot.patch
        nopax.patch
        tk.patch"
builddir="$srcdir/$pkgname-v$pkgver-src"

build() {
	cd "$builddir/pypy/goal"
    python2 ../../rpython/bin/rpython --opt=jit --nopax targetpypystandalone.py 
}

check() {
	cd "$builddir/lib-python/3/test/"
	# XXX: Some tests fail because they are implemented using CPython C API
	$builddir/pypy/goal/pypy3-c regrtest.py -q || true
}

package() {
	cd "$builddir/pypy/tool/release"
	./package.py --archive-name $pkgname-$pkgver --builddir "$pkgdir" 
}

sha512sums="d78b4c899a5643028664365ed973a7b292a8e5b3989cc75203cd381ea3cda7dd73121c574726e23dca86e8364fcfcf42c372c9deee438c805f30d6e1c4ac115a  pypy3-v5.8.0-src.tar.bz2
a161dee3ff4090697cf45f7066cb237e4605904b758e14c587a288c4cba96b4311da4fe4c99f054cd97236e0ed46ba11a3071123edf29d2e86a3515d79b7d56c  conststdio.patch
ebd21c43ac61ebc6b597af42adc1af6a85e5ea84018d5f8add69e8abf800aa71798df2ffbda008befbca200d1c5f92b4602c5d128fac07ac31cd2d5877e33cc2  dbremove.patch
3a25c8613033636f18aeb7218eb77a18e7f5c25787dbbff1853d9ffffd6631053b3cd6d6503bcce2df388e625b166f7d982160c61de0a9ba5ffe1d81a5f9dfd2  libpthread.patch
9cac3684e29ac9e56f8b4085fe02ed027fa208963e4f54453898af3eeef1a0f07b6f3d6f940b2dca05e9f1fbff8baede696a57e53ac01fdadc081943a48580da  nomandelbrot.patch
90ef3c95d3775e2518ccc26db62eb0ff3e9ffb30def56f1ccccb001fbe0aa000f9a5b353ce0efced7a983fe542f0f23f38a0b58e76685b53375dea0e822a9df8  nopax.patch
7fa1c5893ae191a93282a085d63fb3f7a7b62c6d365c7b52e0a097517341a8b9097d3a613dba42862879d8a13ea028202a1af74ceafbd334a381b241570e32d4  tk.patch"