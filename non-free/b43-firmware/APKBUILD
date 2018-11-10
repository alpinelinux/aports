# Contributor: Natanael Copa <ncopa@alpinelinux.org>
# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=b43-firmware
pkgver=4.150.10.5
pkgrel=1
pkgdesc="Firmware for b43 driver"
url="http://linuxwireless.org/en/users/Drivers/b43#firmware_installation"
license="propietary"
depends=""
makedepends="b43-fwcutter"
install=
subpackages=
arch="noarch"
source="https://mirror2.openwrt.org/sources/broadcom-wl-$pkgver.tar.bz2"
builddir="$srcdir/broadcom-wl-$pkgver"

build() {
	cd "$builddir"
	install -d "$builddir"/lib/firmware
	b43-fwcutter -w "$builddir"/lib/firmware \
		"$srcdir"/broadcom-wl-$pkgver/driver/wl_apsta_mimo.o
}

package() {
	cd "$builddir"
	for i in lib/firmware/b43/*; do
		install -D "$i" "$pkgdir"/"$i"
	done
}

sha512sums="0ada0cd0446fcc7f753945d1a18eb31844ed45a137df679f2acba85f02e2acb1e3e8bde6241ac9ca2bedee1f74a018682de13fe4e185aced603547a6e03f7a3e  broadcom-wl-4.150.10.5.tar.bz2"
