# Contributor: William Walker <w_walker@icloud.com>
# Maintainer: William Walker <w_walker@icloud.com>
pkgname=zarf
pkgver=0.26.4
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
e6686870853f23cff712b5d7246a61ce46196e515bd8d2e3d94d1aa04b37af05fd005e71c6b24fb8f353c3c990032ad5de935ef1a1b3cec4ad52896aea03e16b  zarf-v0.26.4.tar.gz
"
