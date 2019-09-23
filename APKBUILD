# Maintainer: Díaz Urbaneja Diego <sodomon2@gmail.com>
pkgname=armagetronad
pkgver=0.2.8.3.4
pkgrel=0
pkgdesc="A Tron Clone in 3D."
url="http://armagetronad.net/"
arch="x86 x86_64"
license="GPL"
depends="ftgl libxml2 sdl_image sdl_mixer"
makedepends="sdl_image-dev sdl_mixer-dev ftgl-dev libxml2-dev"
subpackages="$pkgname-doc"
options="!check"
source="https://launchpad.net/$pkgname/0.2.8/$pkgver/+download/$pkgname-$pkgver.src.tar.gz"
builddir="$srcdir/$pkgname-$pkgver"

build() {
  cd "$builddir"
  ./configure \
    --prefix=/usr \
    --disable-useradd \
    --enable-dirty \
    --enable-music \
    --enable-armathentication \
    --enable-master \
    --enable-krawall \
    --bindir=/usr/bin \
    --sbindir=/usr/sbin \
    --libexecdir=/usr/libexec \
    --datadir=/usr/share \
    --sysconfdir=/usr/share \
    --sharedstatedir=/usr/share \
    --localstatedir=/var \
    --libdir=/usr/share \
    --includedir=/usr/share \
    --docdir=/usr/share/doc/armagetronad
  make
}


package() {
  cd "$builddir"
    make DESTDIR="$pkgdir" install
}

sha512sums="b6564a67e37dae095f56fb0b98c5d80e09ea220e7416b549978e3ab4b018eb01026e4a5d339d46aaaf4edc7b5e1d707cf6a172f79ef367414a1c7636f14da234  armagetronad-0.2.8.3.4.src.tar.gz"
