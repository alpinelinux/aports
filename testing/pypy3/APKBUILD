# Contributor: Mihnea Saracin <mihnea.saracin@rinftech.com>
# Maintainer: Mihnea Saracin <mihnea.saracin@rinftech.com>
pkgname=pypy3
pkgver=5.8.0
pkgrel=0
pkgdesc="A fast, compliant alternative implementation of the Python 3 language"
url="https://github.com/HorridBro/AlpinePyPy3"
arch="all"
license="MIT"
depends=""
makedepends="python2 linux-headers libffi-dev pkgconf zlib-dev bzip2-dev 
             sqlite-dev ncurses-dev readline-dev libressl-dev expat-dev 
             gdbm-dev paxmark tk tk-dev py3-cffi py2-cffi xz xz-dev"
subpackages="$pkgname-dev"
source="https://bitbucket.org/pypy/pypy/downloads/$pkgname-v$pkgver-src.tar.bz2
	dbremove.patch
	tk.patch
	conststdio.patch
        libpthread.patch
        nomandelbrot.patch
        nopax.patch"
builddir="$srcdir/$pkgname-v$pkgver-src"

build() {
	cd "$builddir/pypy/goal"
        python2 ../../rpython/bin/rpython --opt=jit --nopax targetpypystandalone.py 
}

check() {
	cd "$builddir/lib-python/3/test/"
	# XXX: Some tests fail because they are implemented using CPython C API
	$builddir/pypy/goal/pypy3-c regrtest.py -q || true
}

package() {
	cd "$builddir/pypy/tool/release"
	./package.py --archive-name $pkgname-$pkgver --builddir "$pkgdir" 
}

sha512sums="d78b4c899a5643028664365ed973a7b292a8e5b3989cc75203cd381ea3cda7dd73121c574726e23dca86e8364fcfcf42c372c9deee438c805f30d6e1c4ac115a  pypy3-v5.8.0-src.tar.bz2
cbb2030be8882ce632367d49c3ec6895369a894c90532d8c3f20b4b0cdbd1b94c5b3473c4c7dcb462fa3961c1aa2488e8240f7dbdf78ac3400bfaa0114c9599d  dbremove.patch
4758cd1ab96dbf1f3ced4553c04adb62b2aaf1528e0d98c8650fc489b7b09281187961662c94039a5303f3bfa8603ac10bce5a96f5d83a74cd6fc137f6afcb23  conststdio.patch
1f4dd4a8ad21619e93b6450b28e3733b7299862bed31764d5084105b3fb1d8a4accbd6037643a3eb15f6edafb8e764e16331975dd7c217782c0d11be24e6b7c5  libpthread.patch
2beb51cada368b041bc585c8196dea00bd8884a25ded376ac8fa097e79044405dfbd95ce1b6626f8e81cd4f950112d04ae98d5adf3b0e9e7327934419243e90d  nomandelbrot.patch
8051547cea7182ff6c191f9b3c05f11768977d65877c10ffa202baad91e27731a43844cc269ccd99c78e61ff60d95294adc997937a532a9f6d54eb8e3009eeef  nopax.patch
264de3a068f5680488574464eebb1a88d5590b257749b6ce1a75ec5bc498fa9609500a4ca92d82d370613286a0a4217f4a1667311f73479fa58b138715878711  tk.patch"
