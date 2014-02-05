# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-keys
pkgver=1.1
pkgrel=0
pkgdesc="Public keys for Alpine Linux packages"
url="http://alpinelinux.org"
arch="noarch"
license="GPL"
depends=""
makedepends=""
install=""
subpackages=""
replaces="alpine-base"
source="http://dev.alpinelinux.org/~ncopa/alpine/alpine-devel@lists.alpinelinux.org-4a6a0840.rsa.pub
	alpine-devel@lists.alpinelinux.org-4d07755e.rsa.pub
	alpine-devel@lists.alpinelinux.org-5243ef4b.rsa.pub
	alpine-devel@lists.alpinelinux.org-524d27bb.rsa.pub
	alpine-devel@lists.alpinelinux.org-5261cecb.rsa.pub
	buildozer-50d1ba71.rsa.pub"

build() {
	return 0
}

package() {
	# copy keys for repos
	mkdir -p "$pkgdir"/etc/apk/keys
	install -m644 "$srcdir"/alpine-devel*.pub \
		"$pkgdir"/etc/apk/keys/ || return 1
}

md5sums="75ee19ea2b03c12bc171647edc677f6f  alpine-devel@lists.alpinelinux.org-4a6a0840.rsa.pub
ca7d06006181b625cf1ff4aefd51bd08  alpine-devel@lists.alpinelinux.org-4d07755e.rsa.pub
056daa8bf61a95a42971bf6c13bf300f  buildozer-50d1ba71.rsa.pub"
