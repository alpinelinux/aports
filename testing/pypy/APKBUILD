# Contributor: Mihnea Saracin <mihnea.saracin@rinftech.com>
# Maintainer: Mihnea Saracin <mihnea.saracin@rinftech.com>
pkgname=pypy
pkgver=5.7.1
pkgrel=0
pkgdesc="A fast, compliant alternative implementation of the Python 2 language"
url="https://bitbucket.org/pypy"
arch="all"
license="MIT"
depends="libssl1.0 libcrypto1.0 libffi libbz2 "
makedepends="python linux-headers libffi-dev pkgconf zlib-dev bzip2-dev sqlite-dev ncurses-dev readline-dev libressl-dev expat-dev gdbm-dev tk tk-dev py2-cffi paxmark"
subpackages="$pkgname-dev "
source="https://bitbucket.org/$pkgname/$pkgname/downloads/pypy2-v$pkgver-src.tar.bz2
        alpine.patch"
builddir="$srcdir/pypy2-v$pkgver-src"



build() {
        cd "$builddir/pypy/goal"
        python2 ../../rpython/bin/rpython --opt=jit --nopax targetpypystandalone.py 
}



package() {
        cd "$builddir/pypy/tool/release"
        ./package.py --archive-name pypy2-$pkgver --builddir "$pkgdir" 
}



sha512sums="1ad2dddb40c28d2d3e95a9f0730e765d981dee6e2d0664cf1274eb7c1021690a848c3485c846eac8a8b64425b44946b5b2d223058ec4699155a2122ee7d38b75  pypy2-v5.7.1-src.tar.bz2
11ed0fc38203fdd4c1f468414e0b4b103c7d140751a0aca43878afbb42333461b8bb92f9adafbceb611e46387694a0bbd5f9e86c104362a47dd95077cc4bb769  alpine.patch"

