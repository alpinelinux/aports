# Contributor: Matt Aitchison <Matt.Aitchison@gliderlabs.com>
# Maintainer: Randall Leeds <randall@bleeds.info>
pkgname=entrykit
pkgver=0.4.0
pkgrel=0
pkgdesc="Entrypoint tools for elegant, programmable containers"
url="https://github.com/progrium/entrykit"
arch="all"
license="MIT"
depends=""
depends_dev=""
makedepends="go"
install="$pkgname.post-install $pkgname.post-deinstall"
subpackages=""
source="$pkgname-$pkgver.tar.gz::https://github.com/progrium/$pkgname/archive/v$pkgver.tar.gz"


_builddir="$srcdir/go/src/github.com/progrium/$pkgname"
build() {
	export GOPATH="$srcdir/go"
	mkdir -p $_builddir
	ln -sf $srcdir/$pkgname-$pkgver/* "$_builddir"

	cd "$_builddir"
	go get -d -v ./cmd || return 1
	go build -a -ldflags "-X main.Version=$pkgver" -o bin/$pkgname ./cmd || return 1
}

package() {
	install -Dm755 "$_builddir/bin/$pkgname" "$pkgdir/usr/bin/$pkgname"
}

md5sums="10d9189782ca67a2a7de963ef05dd861  entrykit-0.4.0.tar.gz"
sha256sums="89a35754ff0a9beba3a6d2eb34bc6048476409536586908cf3e2c6afdb503463  entrykit-0.4.0.tar.gz"
sha512sums="79097b090af7bcaed31587d7ea81776698ac1bfa75defb4269e02da3f282d109fa1b68b3e8aca4351a3eb1fe1c0128c26853174f398b8d015b16a51a131ca6ca  entrykit-0.4.0.tar.gz"
