# Contributor: Alex Yam <alex@alexyam.com>
maintainer="Sertonix <sertonix@posteo.net>"
pkgname=sane-airscan
pkgver=0.99.36
pkgrel=0
pkgdesc="SANE backend for AirScan (eSCL) and WSD document scanners"
url="https://github.com/alexpevzner/sane-airscan"
arch="all"
license="GPL-2.0-or-later"
makedepends="
	avahi-dev
	gnutls-dev
	libjpeg-turbo-dev
	libpng-dev
	libxml2-dev
	meson
	sane-dev
	tiff-dev
	"
subpackages="$pkgname-doc"
source="$pkgname-$pkgver.tar.gz::https://github.com/alexpevzner/sane-airscan/archive/refs/tags/$pkgver.tar.gz"

build() {
	abuild-meson . output
	meson compile -C output
}

check() {
	meson test --print-errorlogs -C output
}

package() {
	DESTDIR="$pkgdir" meson install --no-rebuild -C output
}

sha512sums="
a87f9ede123710b4be9d0bbf950487abf71ddc9691020d2defadf99eae12afc78b4d99df834c4f84984bc2cae4e3190fa4dac7a44518a3445f263aa4ad37ea67  sane-airscan-0.99.36.tar.gz
"
