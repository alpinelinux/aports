#-*-mode: Shell-script; coding: utf-8;-*-
# Maintainer: Mitch Tishmack <mitch.tishmack@gmail.com>
pkgname=stack
pkgver=1.3.2
pkgrel=0
pkgdesc="The Haskell Tool Stack"
url="https://github.com/commercialhaskell/stack"
arch="x86_64 armhf"
license="bsd3"
depends="ca-certificates"
makedepends="bash ghc cabal pcre-dev gmp-dev zlib-dev linux-headers"
install=""
source="
	$pkgname-$pkgver.tar.gz::https://github.com/commercialhaskell/stack/archive/v$pkgver.tar.gz
	cabal.config
"
builddir="$srcdir/$pkgname-$pkgver"

build() {
	local buildtmp="$builddir/tmp"
	cd "$builddir"
	install -d "$buildtmp"
	(
		export PATH="${PATH}:$buildtmp/.cabal/bin"
		export HOME="$buildtmp"
		cabal update || return 1
		cp "$srcdir/cabal.config" cabal.config || return 1
		cabal install --force-reinstalls -fstatic --ghc-option=-fllvm  || return 1
	) || return 1
}

package() {
	install -Dm755 "$builddir/tmp/.cabal/bin/$pkgname" "$pkgdir/usr/bin/$pkgname" || return 1
}
md5sums="84e1a650cdbb2a4282960462477189b5  stack-1.3.2.tar.gz
b23c69a230deb65fb993fe82ee5fdd59  cabal.config"
sha256sums="12e65e31e5e7bd2d1094505eaab7a1d8c2f5422e0dc17bb6b4b50bebb857cc71  stack-1.3.2.tar.gz
0af9ac97442835b8701dd145bc925e60747d19506d075c9916bd852e0507025e  cabal.config"
sha512sums="306f3755739069033a826bb80f3aacd668156a5051ba809fa08f9cc25b8cbc99266723ea0eb486e1847d80f0f8156a4525267efb410d445cbe4aebf901a8f62a  stack-1.3.2.tar.gz
195fe4aeef647ead3bdf35c9dfbb374a0f6da6dae478cc1bd5f807a2b5b4524b99c6080feb74cee5a44a3bc842f4a2d9fbad7730f78579e969f1099a7f9d5a48  cabal.config"
