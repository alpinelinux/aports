# Contributor: William Walker <w_walker@icloud.com>
# Maintainer: William Walker <w_walker@icloud.com>
pkgname=zarf
pkgver=0.26.2
pkgrel=0
pkgdesc="DevSecOps for Air Gap & Limited-Connection Systems"
url="https://zarf.dev/"
# not useful/supported elsewhere
arch="aarch64 x86_64"
license="Apache-2.0"
makedepends="go nodejs npm"
source="$pkgname-v$pkgver.tar.gz::https://github.com/defenseunicorns/zarf/archive/refs/tags/v$pkgver.tar.gz"
# tests are integration tests that need a full setup
options="net !check"

prepare() {
	default_prepare

	npm --prefix src/ui ci
}

build() {
	npm --prefix src/ui run build

	local ldflags="
		-X github.com/defenseunicorns/zarf/src/config.CLIVersion=v$pkgver
		-X k8s.io/component-base/version.gitVersion=v0.0.0+zarfv$pkgver
		-X k8s.io/component-base/version.gitCommit=alpine
		-X k8s.io/component-base/version.buildDate=null
		"
	go build -ldflags "$ldflags" -o build/zarf main.go
}

package() {
	install -Dm755 build/zarf -t "$pkgdir"/usr/bin
}

sha512sums="
6aefc398bc0f864b8371905b06627962c487b6a004501bf0e5cc1210167a765171216d175a4dbec830bda82f498cb7b1a4bda53a5047da150670d5eb9e09fcf5  zarf-v0.26.2.tar.gz
"
