# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-keys
pkgver=1.2
pkgrel=0
pkgdesc="Public keys for Alpine Linux packages"
url="http://alpinelinux.org"
arch="noarch"
license="MIT"
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
	alpine-devel@lists.alpinelinux.org-57a8ad0e.rsa.pub"

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
db1b0e718ae11127bc3a2485cfd6f4af  alpine-devel@lists.alpinelinux.org-5243ef4b.rsa.pub
c227288cab4154514a4aa89efab20dba  alpine-devel@lists.alpinelinux.org-524d27bb.rsa.pub
01e59112f4b4a9d8dc36b6203b678fca  alpine-devel@lists.alpinelinux.org-5261cecb.rsa.pub
7f586ce74e9d6e4bf2ff302df796d058  alpine-devel@lists.alpinelinux.org-57a8ad0e.rsa.pub"
sha256sums="9c102bcc376af1498d549b77bdbfa815ae86faa1d2d82f040e616b18ef2df2d4  alpine-devel@lists.alpinelinux.org-4a6a0840.rsa.pub
2adcf7ce224f476330b5360ca5edb92fd0bf91c92d83292ed028d7c4e26333ab  alpine-devel@lists.alpinelinux.org-4d07755e.rsa.pub
ebf31683b56410ecc4c00acd9f6e2839e237a3b62b5ae7ef686705c7ba0396a9  alpine-devel@lists.alpinelinux.org-5243ef4b.rsa.pub
1bb2a846c0ea4ca9d0e7862f970863857fc33c32f5506098c636a62a726a847b  alpine-devel@lists.alpinelinux.org-524d27bb.rsa.pub
12f899e55a7691225603d6fb3324940fc51cd7f133e7ead788663c2b7eecb00c  alpine-devel@lists.alpinelinux.org-5261cecb.rsa.pub
a1b140413d283633504a8932af944b86acd640d0b7dbd5bce0fdab21e5d8daac  alpine-devel@lists.alpinelinux.org-57a8ad0e.rsa.pub"
sha512sums="2d4064cbe09ff958493ec86bcb925af9b7517825d1d9d8d00f2986201ad5952f986fea83d1e2c177e92130700bafa8c0bff61411b3cdb59a41e460ed719580a6  alpine-devel@lists.alpinelinux.org-4a6a0840.rsa.pub
85af435d36c3cf0ba783dc70628d0060f7fae8b1543995610afceaeb2183d3fa846203f69825487f1f838d7d1315da015f02a44341eebdd2f45fbcd03620bd0a  alpine-devel@lists.alpinelinux.org-4d07755e.rsa.pub
e18e65ee911eb1f8ea869f758e8f2c94cf2ac254ee7ab90a3de1d47b94a547c2066214abf710da21910ebedc0153d05fd4fe579cc5ce24f46e0cfd29a02b1a68  alpine-devel@lists.alpinelinux.org-5243ef4b.rsa.pub
698fda502f70365a852de3c10636eadfc4f70a7a00f096581119aef665e248b787004ceef63f4c8cb18c6f88d18b8b1bd6b3c5d260e79e6d73a3cc09537b196e  alpine-devel@lists.alpinelinux.org-524d27bb.rsa.pub
721134f289ab1e7dde9158359906017daee40983199fe55f28206c8cdc46b8fcf177a36f270ce374b0eba5dbe01f68cbb3e385ae78a54bb0a2ed1e83a4d820a5  alpine-devel@lists.alpinelinux.org-5261cecb.rsa.pub
ec76754bb76577764e2f3498b24be6830266d71b6e12f54ee77ae6a18d30eb6e824a371c89e612cdde08bdc495161cc69147f5374905ae3774433868f9ee8a75  alpine-devel@lists.alpinelinux.org-57a8ad0e.rsa.pub"
