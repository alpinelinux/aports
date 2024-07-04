# Contributor: Natanael Copa <ncopa@alpinelinux.org>
# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-base
pkgver=3.21.0_alpha20240606
pkgrel=0
pkgdesc="Meta package for minimal alpine base"
url="https://alpinelinux.org"
arch="noarch"
license="MIT"
depends="
	alpine-baselayout
	alpine-conf
	alpine-release
	apk-tools
	busybox
	busybox-mdev-openrc
	busybox-openrc
	busybox-suid
	musl-utils
	openrc
	"
subpackages="alpine-release:release"
replaces="alpine-baselayout"
options="!check"

package() {
	mkdir -p "$pkgdir"
}

release() {
	depends="alpine-keys"
	pkgdesc="Alpine release data"

	mkdir -p "$subpkgdir"/etc
	# create /etc/alpine-release
	echo $pkgver > "$subpkgdir"/etc/alpine-release
	local _ver="$(echo "$pkgver" | grep -E -o '^[0-9]+\.[0-9]+')"
	local _rel="v$_ver"
	case "$pkgver" in
	*_alpha*|*_beta*|*_pre*)
		_ver="$pkgver (edge)"
		_rel="edge"
		;;
	esac

	# create /etc/issue
	cat >"$subpkgdir"/etc/issue<<EOF
Welcome to Alpine Linux $_ver
Kernel \\r on an \\m (\\l)

EOF

	# create os-release
	cat >"$subpkgdir"/etc/os-release<<EOF
NAME="Alpine Linux"
ID=alpine
VERSION_ID=$pkgver
PRETTY_NAME="Alpine Linux $_rel"
HOME_URL="https://alpinelinux.org/"
BUG_REPORT_URL="https://gitlab.alpinelinux.org/alpine/aports/-/issues"
EOF

	# create secfixes.d repository list
	mkdir -p "$subpkgdir"/etc/secfixes.d
	cat >"$subpkgdir"/etc/secfixes.d/alpine<<EOF
https://secdb.alpinelinux.org/$_rel/main.json
https://secdb.alpinelinux.org/$_rel/community.json
EOF
}
