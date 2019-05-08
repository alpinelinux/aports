# Contributor: Natanael Copa <ncopa@alpinelinux.org>
# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-base
pkgver=3.10_beta20190508
pkgrel=0
pkgdesc="Meta package for minimal alpine base"
url="https://alpinelinux.org"
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
	local _ver="$(echo "$pkgver" | grep -E -o '^[0-9]+\.[0-9]+')"
	local _rel="v$_ver"
	case "$pkgver" in
	*_alpha*|*_beta*|*_pre*)
		_ver="$pkgver (edge)"
		_rel="edge"
		;;
	esac

	# create /etc/issue
	cat >"$pkgdir"/etc/issue<<EOF
Welcome to Alpine Linux $_ver
Kernel \\r on an \\m (\\l)

EOF

	# create os-release
	cat >"$pkgdir"/etc/os-release<<EOF
NAME="Alpine Linux"
ID=alpine
VERSION_ID=$pkgver
PRETTY_NAME="Alpine Linux $_rel"
HOME_URL="https://alpinelinux.org/"
BUG_REPORT_URL="https://bugs.alpinelinux.org/"
EOF
}
