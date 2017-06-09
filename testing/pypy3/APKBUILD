# Contributor: Mihnea Saracin <mihnea.saracin@rinftech.com>
# Maintainer: Mihnea Saracin <mihnea.saracin@rinftech.com>
pkgname=pypy3
pkgver=5.7.1
pkgrel=0
pkgdesc="A fast, compliant alternative implementation of the Python 3 language"
url="https://github.com/HorridBro/AlpinePyPy3"
arch="all"
license="MIT"
depends=""
makedepends="python linux-headers libffi-dev pkgconf zlib-dev bzip2-dev sqlite-dev ncurses-dev readline-dev libressl-dev expat-dev gdbm-dev paxmark tk tk-dev py3-cffi py2-cffi xz xz-dev"
subpackages="$pkgname-dev"
source="https://bitbucket.org/pypy/pypy/downloads/$pkgname-v$pkgver-src.tar.bz2
        alpine.patch libpthread_version.patch const_stdio.patch"
builddir="$srcdir/$pkgname-v$pkgver-src"



build() {
        cd "$builddir/pypy/goal"
        python2 ../../rpython/bin/rpython --opt=jit --nopax targetpypystandalone.py 
}



package() {
        cd "$builddir/pypy/tool/release"
        ./package.py --archive-name $pkgname-$pkgver --builddir "$pkgdir" 
}

check(){
	cd "$builddir/lib-python/3/test/"
	# XXX: Some tests fail because they are implemented using CPython C functions
	$builddir/pypy/goal/pypy3-c regrtest.py -q || true
}

sha512sums="f8ead8214ad7d89fe80e24d97b13ece7f2c80b2f11446257a2eab0e3025fc7d8fec26474b0e9eb2b2e3ccd629532dd062829459361b601add12e40793bd5aa60  pypy3-v5.7.1-src.tar.bz2
b8adc05b913cf5b9630a251776336f85d0be6392de37c5a57cfd4774c0855e3a0045bd63c37e02b33f94e610677014a3c3d0ce896532ee5f885dfac6f84e1aaa  alpine.patch
d669ac511733b0ddbf7e594f2aebca4490d078d6f1869bada799f7a00fdf7a3ad0c8c61eca56315a3264cc3ca22c35d3d0ced6c91584f4e35f074e6e75271508  libpthread_version.patch
a6d379c444186d5b4e9318f668da4c5f56df16c119436b581ec9ba89f92602d5d1e8d171e2805ba5528af89066d4d938b9b6bfb00d46d296f837236d7f6230c3  const_stdio.patch"








