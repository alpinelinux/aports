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
             gdbm-dev paxmark tk tk-dev py2-cffi xz xz-dev"
subpackages="$pkgname-dev $pkgname-doc"
source="https://bitbucket.org/pypy/pypy/downloads/$pkgname-v$pkgver-src.tar.bz2
	dbremove.patch
	tk.patch
	conststdio.patch
	nomandelbrot.patch
        pass_through_noop_confstr.patch
	dont_redefine_mismatch_constants.patch
	"
builddir="$srcdir/$pkgname-v$pkgver-src"

build() {
	cd "$builddir/pypy/goal"
	python2 ../../rpython/bin/rpython --opt=jit targetpypystandalone.py
        paxmark -zm ./pypy3-c
        PYTHONPATH=../.. ./pypy3-c ../tool/build_cffi_imports.py
}

check() {
        local pypy3="$builddir/pypy/goal/pypy3-c"
        $pypy3 smoketest.py | grep "hello world"
        $pypy3 pystone.py
}


package() {
        cd "$builddir"
        python2 pypy/tool/release/package.py --archive-name pypy3 --targetdir .
        mkdir -p "$pkgdir/usr"
        tar xf pypy3.tar.bz2 -C "$pkgdir/usr"
        mkdir -p "$pkgdir/usr/bin"
        ln -s "/usr/pypy3/bin/pypy3" "$pkgdir/usr/bin/pypy3"
        mkdir -p "$pkgdir/usr/lib"
        mv "$pkgdir/usr/pypy3/bin/libpypy3-c.so" "$pkgdir/usr/lib/libpypy3-c.so"
        paxmark -zm "$pkgdir/usr/pypy3/bin/pypy3"
}

doc() {
        local docdir="$subpkgdir/usr/share/doc/$pkgname"
        mkdir -p "$docdir"
        mv "$pkgdir/usr/pypy3/LICENSE" "$docdir"
        mv "$pkgdir/usr/pypy3/README.rst" "$docdir"
}

dev() {
        # Note: all the files in lib_pypy might be needed at runtime
        mkdir -p "$subpkgdir/usr/pypy3"
        mv "$pkgdir/usr/pypy3/include" "$subpkgdir/usr/pypy3/"
        mkdir -p "$subpkgdir/usr/include"
        ln -s "/usr/pypy3/include" "$subpkgdir/usr/include/pypy3"
}


sha512sums="d78b4c899a5643028664365ed973a7b292a8e5b3989cc75203cd381ea3cda7dd73121c574726e23dca86e8364fcfcf42c372c9deee438c805f30d6e1c4ac115a  pypy3-v5.8.0-src.tar.bz2
cbb2030be8882ce632367d49c3ec6895369a894c90532d8c3f20b4b0cdbd1b94c5b3473c4c7dcb462fa3961c1aa2488e8240f7dbdf78ac3400bfaa0114c9599d  dbremove.patch
264de3a068f5680488574464eebb1a88d5590b257749b6ce1a75ec5bc498fa9609500a4ca92d82d370613286a0a4217f4a1667311f73479fa58b138715878711  tk.patch
eb5f4d6cf24f16e4ee20ddcbc30258031863aa5ebefb1925f2a189c8fe457a691d6cf50607672a496270d2edb933d9e0255afca95273f460cbb6a75b9b15cb71  pass_through_noop_confstr.patch
2beb51cada368b041bc585c8196dea00bd8884a25ded376ac8fa097e79044405dfbd95ce1b6626f8e81cd4f950112d04ae98d5adf3b0e9e7327934419243e90d  nomandelbrot.patch
f402296b263f3f9ee06529127d98aa49caddda00012997725da1fc512bc4ba1bf7b9d494cc599b2ec7b528038bd035397083f715166da8ae0a65a7e57bfa3762  dont_redefine_mismatch_constants.patch
4758cd1ab96dbf1f3ced4553c04adb62b2aaf1528e0d98c8650fc489b7b09281187961662c94039a5303f3bfa8603ac10bce5a96f5d83a74cd6fc137f6afcb23  conststdio.patch"
