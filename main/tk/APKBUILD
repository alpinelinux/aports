# Maintainer:
pkgname=tk
pkgver=8.6.10
pkgrel=1
pkgdesc="GUI toolkit for the Tcl scripting language"
url="http://tcl.sourceforge.net/"
arch="all"
options="!check"  # Requires a running X11 server.
license="TCL"
depends_dev="tcl-dev libx11-dev libxft-dev fontconfig-dev"
makedepends="$depends_dev libpng-dev"
subpackages="$pkgname-doc $pkgname-dev"
source="https://downloads.sourceforge.net/sourceforge/tcl/tk$pkgver-src.tar.gz
	"

_major=${pkgver%.*}
builddir="$srcdir"/tk$pkgver/unix

build() {
	local _64bit="--disable-64bit"
	case "$CARCH" in
		x86_64) _64bit="--enable-64bit";;
	esac

	./configure \
		--build=$CBUILD \
		--host=$CHOST \
		--prefix=/usr \
		--mandir=/usr/share/man \
		$_64bit
	make
}

package() {
	export LD_LIBRARY_PATH="$builddir"
	make -j1 INSTALL_ROOT="$pkgdir" install install-private-headers

	ln -sf wish${_major} "$pkgdir"/usr/bin/wish
	install -Dm644 ../license.terms \
		$pkgdir/usr/share/licenses/$pkgname/LICENSE

	# remove buildroot traces
	find "$pkgdir" -name '*Config.sh' | xargs sed -i -e "s#${srcdir}#/usr/src#"

	# move demos to -doc directory
	mkdir -p "$pkgdir"/usr/share/doc/$pkgname/examples/
	mv "$pkgdir"/usr/lib/tk$_major/demos/ \
		"$pkgdir"/usr/share/doc/$pkgname/examples/
}

dev() {
	# libtk8.6.so is the only library made and it remains in the mainpackage
	# so depending on the main package automatically is never done
	# https://gitlab.alpinelinux.org/alpine/aports/issues/11232#note_68789
	depends_dev="$depends_dev tk=$pkgver-r$pkgrel"
	default_dev
	cd $pkgdir
	for i in $(find . -name '*.c' -o -name '*Config.sh'); do
		mkdir -p "$subpkgdir"/${i%/*}
		mv $i "$subpkgdir"/${i%/*}/
	done
}

sha512sums="d12ef3a5bde9e10209a24e9f978bd23360a979d8fa70a859cf750a79ca51067a11ef6df7589303b52fe2a2baed4083583ddaa19e2c7cb433ea523639927f1be5  tk8.6.10-src.tar.gz"
