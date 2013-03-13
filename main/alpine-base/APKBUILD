# Contributor: Natanael Copa <ncopa@alpinelinux.org>
# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-base
pkgver=2.6.0_alpha5_git20130313
pkgrel=0
pkgdesc="Meta package for minimal alpine base"
url="http://alpinelinux.org"
arch="noarch"
license="GPL"
depends="alpine-baselayout alpine-conf apk-tools busybox busybox-initscripts
	openrc libc-utils"
makedepends=
install=
subpackages=
replaces="alpine-baselayout"
source="http://dev.alpinelinux.org/~ncopa/alpine/alpine-devel@lists.alpinelinux.org-4a6a0840.rsa.pub
	alpine-devel@lists.alpinelinux.org-4d07755e.rsa.pub
	buildozer-50d1ba71.rsa.pub"

build() {
	return 0
}

package() {
	# copy keys for repos
	mkdir -p "$pkgdir"/etc/apk/keys
	install -m644 "$srcdir"/alpine-devel*.pub \
		"$pkgdir"/etc/apk/keys/ || return 1
	if [ "$ALPINE_LIBC" = "eglibc" ]; then
		install -m644 "$srcdir"/buildozer-50d1ba71.rsa.pub \
			"$pkgdir"/etc/apk/keys/ || return 1
	fi
	# create /etc/alpine-release
	echo $pkgver > "$pkgdir"/etc/alpine-release

	# create /etc/issue
	cat >"$pkgdir"/etc/issue<<EOF
Welcome to Alpine Linux ${pkgver%.*}
Kernel \\r on an \\m (\\l)

EOF
}

md5sums="75ee19ea2b03c12bc171647edc677f6f  alpine-devel@lists.alpinelinux.org-4a6a0840.rsa.pub
ca7d06006181b625cf1ff4aefd51bd08  alpine-devel@lists.alpinelinux.org-4d07755e.rsa.pub
056daa8bf61a95a42971bf6c13bf300f  buildozer-50d1ba71.rsa.pub"
