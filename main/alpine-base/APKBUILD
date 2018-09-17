# Contributor: Natanael Copa <ncopa@alpinelinux.org>
# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-base
pkgver=3.8.0
pkgrel=0
pkgdesc="Meta package for minimal alpine base"
url="http://alpinelinux.org"
arch="noarch"
license="MIT"
depends="alpine-baselayout alpine-conf apk-tools busybox busybox-suid busybox-initscripts
	openrc libc-utils alpine-keys"
makedepends=""
install=""
subpackages=""
replaces="alpine-baselayout"
source=""

build() {
	return 0
}

package() {
	mkdir -p "$pkgdir"/etc
	# create /etc/alpine-release
	echo $pkgver > "$pkgdir"/etc/alpine-release

	# create /etc/issue
	cat >"$pkgdir"/etc/issue<<EOF
Welcome to Alpine Linux ${pkgver%.*}
Kernel \\r on an \\m (\\l)

EOF

	_ver="$(echo "$pkgver" | grep -E -o '^[0-9]+\.[0-9]+')"
	# create os-release
	cat >"$pkgdir"/etc/os-release<<EOF
NAME="Alpine Linux"
ID=alpine
VERSION_ID=$pkgver
PRETTY_NAME="Alpine Linux v$_ver"
HOME_URL="http://alpinelinux.org"
BUG_REPORT_URL="http://bugs.alpinelinux.org"
EOF
}
