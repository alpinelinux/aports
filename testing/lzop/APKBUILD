# Contributor: Sasha Gerrand <alpine-pkgs@sgerrand.com>
# Maintainer: Sasha Gerrand <alpine-pkgs@sgerrand.com>
pkgname=lzop
pkgver=1.04
pkgrel=0
pkgdesc="lzop is a very fast file compressor"
url="https://www.lzop.org/"
arch="all"
license="GPL-2.0-only"
makedepends="lzo-dev"
subpackages="$pkgname-doc"
source="https://www.lzop.org/download/lzop-$pkgver.tar.gz"

build() {
        cd "$builddir"
        ./configure \
                --build=$CBUILD \
                --host=$CHOST \
                --prefix=/usr \
                --sysconfdir=/etc \
                --mandir=/usr/share/man \
                --localstatedir=/var
        make
}

check() {
        cd "$builddir"
        make check
}

package() {
        cd "$builddir"
        make DESTDIR="$pkgdir" install
}

sha512sums="5829b4495ffefab549aa697a05c536ce593c572c9eee6004460583a0090abcd317c6074c4f981dfee6be61ac8d127f02dd37053b6cb782af64db41586a8bbb6e  lzop-1.04.tar.gz"
