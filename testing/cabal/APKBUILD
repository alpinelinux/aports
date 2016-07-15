#-*-mode: Shell-script; coding: utf-8;-*-
# Maintainer: Mitch Tishmack <mitch.tishmack@gmail.com>
pkgname=cabal
pkgver=1.24.0.2
pkgrel=0
pkgdesc="The Haskell Cabal"
url="http://haskell.org"
arch="x86_64 armhf"
license="bsd3"
depends="musl zlib gmp"
makedepends="ghc gmp-dev libffi-dev zlib-dev binutils-gold chrpath"
install=""
subpackages="$pkgname-doc"
source="
	https://www.haskell.org/$pkgname/release/$pkgname-install-$pkgver/$pkgname-install-$pkgver.tar.gz
	cabal-0001-force-ld.gold.patch
"
builddir="$srcdir/$pkgname-install-$pkgver"

build() {
	cd "$builddir"
	(
		export HOME="$builddir"
		export NO_DOCUMENTATION=1
		export EXTRA_BUILD_OPTS="--ghc-option=-fllvm"
		./bootstrap.sh || return 1
	) || return 1
}

doc() {
	default_doc
	install -Dm644 "$builddir/LICENSE" "$subpkgdir/usr/share/licenses/$pkgname/LICENSE" || return 1
}

package() {
	install -Dm755 "$builddir/dist/build/cabal/cabal" "$pkgdir/usr/bin/cabal" || return 1
	chrpath -d "$pkgdir/usr/bin/cabal" || return 1
}
md5sums="2577c3d7712a74614bf7cc63a5341cab  cabal-install-1.24.0.2.tar.gz
dbdcab13dfd1c0afac66ad5213885fc0  cabal-0001-force-ld.gold.patch"
sha256sums="2ac8819238a0e57fff9c3c857e97b8705b1b5fef2e46cd2829e85d96e2a00fe0  cabal-install-1.24.0.2.tar.gz
9b376e1743e4a8cc6b422bd6cef6c08a4e74d83ec2712a4d5a66c2aaccb482d6  cabal-0001-force-ld.gold.patch"
sha512sums="bd055a52ff0ac697e6f21a588d53dd811d50ee9410659a242c00a5665b360ef10c024df4872b9070c33aa49f779c8817b883b40087d3f4e0be4096a54b2ad5f0  cabal-install-1.24.0.2.tar.gz
cb83ea703c3d0d5124a9f35606250c5bc71b67c141512bcdaa285b8a9bb99c5ca34ad1c3cf0c73ea0486215bb46285989ada4967f7e7a972e58cefdbc4eab8a9  cabal-0001-force-ld.gold.patch"
