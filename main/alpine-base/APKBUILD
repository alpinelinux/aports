# Contributor: Natanael Copa <ncopa@alpinelinux.org>
# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-base
pkgver=2.0
pkgrel=3
pkgdesc="Meta package for minimal alpine base"
url="http://alpinelinux.org"
license="GPL"
depends="alpine-baselayout alpine-conf apk-tools busybox busybox-initscripts
	openrc uclibc-utils bbsuid"
makedepends=
install=
subpackages=
source="http://dev.alpinelinux.org/~ncopa/alpine/alpine-devel@lists.alpinelinux.org-4a6a0840.rsa.pub
	"

build() {
	mkdir -p "$pkgdir"/etc/apk/keys
	install -m644 "$srcdir"/*.pub "$pkgdir"/etc/apk/keys/
}
md5sums="75ee19ea2b03c12bc171647edc677f6f  alpine-devel@lists.alpinelinux.org-4a6a0840.rsa.pub"
