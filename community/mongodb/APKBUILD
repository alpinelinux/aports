# Maintainer: Marc Vertes <mvertes@free.fr>
# Contributor: Filipp Andronov <filipp.andronov@gmail.com>
pkgname=mongodb
pkgver=3.6.7
pkgrel=0
pkgdesc='A high-performance, open source, schema-free document-oriented database'
url='http://www.mongodb.org'
arch='x86_64'
options="!check" # out of disk space (>35GB)
license='AGPL3'
pkgusers="mongodb"
pkggroups="mongodb"
depends=
makedepends="scons py-setuptools py2-cheetah py2-typing py2-yaml paxmark
	libressl-dev pcre-dev snappy-dev boost-dev asio-dev libpcap-dev
	snowball-dev zlib-dev cyrus-sasl-dev yaml-cpp-dev"
install="$pkgname.pre-install"
source="http://downloads.mongodb.org/src/mongodb-src-r${pkgver}.tar.gz
	fix-backtrace.patch
	fix-default-stacksize.patch
	fix-elf-native-class.patch
	fix-processinfo_linux.patch
	fix-resolv.patch
	fix-strerror_r.patch
	wiredtiger.patch

	mongodb.confd
	mongodb.initd
	mongodb.logrotate
	mongos.confd
	mongos.initd
	"

builddir="$srcdir"/$pkgname-src-r$pkgver
_buildopts="
	--allocator=system \
	--disable-warnings-as-errors \
	--use-system-boost \
	--use-system-pcre \
	--use-system-stemmer \
	--use-system-snappy \
	--use-system-zlib \
	--use-system-yaml \
	--use-sasl-client \
	--ssl \
	"

build() {
	cd "$builddir"

	export SCONSFLAGS="$MAKEFLAGS"
	scons $_buildopts --prefix=$pkgdir/usr core
}

check() {
	cd "$builddir"

	export SCONSFLAGS="$MAKEFLAGS"
	scons $_buildopts unittests
	python buildscripts/resmoke.py --suites=unittests --jobs=2
}

package() {
	cd "$builddir"

	# install mongo targets
	export SCONSFLAGS="$MAKEFLAGS"
	scons $_buildopts --prefix=$pkgdir/usr install

	# java jit requires paxmark
	paxmark -m "$pkgdir"/usr/bin/mongo*

	# install alpine specific files
	install -dm700 "$pkgdir/var/lib/mongodb"
	install -dm755 "$pkgdir/var/log/mongodb"
	chown mongodb:mongodb "$pkgdir/var/lib/mongodb" "$pkgdir/var/log/mongodb"

	install -Dm755 "$srcdir/mongodb.initd" "$pkgdir/etc/init.d/mongodb"
	install -Dm644 "$srcdir/mongodb.confd" "$pkgdir/etc/conf.d/mongodb"
	install -Dm644 "$srcdir/mongodb.logrotate" "$pkgdir/etc/logrotate.d/mongodb"

	install -Dm755 "$srcdir/mongos.initd" "$pkgdir/etc/init.d/mongos"
	install -Dm644 "$srcdir/mongos.confd" "$pkgdir/etc/conf.d/mongos"
}

sha512sums="6d91785c5569f8b99ff7234fa94c7b87406489b1e04a9e1b44a2af6cbaf79e3685dbc361162935dc4b92bd07ed40bf024dba4ca85c1a4079dd22f1cdac91d594  mongodb-src-r3.6.7.tar.gz
05c4331d028eb396e6cf52d96cdaa2af7199a03522e1a8211df2d36cb053ec093a51e9abf83c4dc00c09a0b1fa119a79bcc719fbc81a48f50ca1527c26613cf0  fix-backtrace.patch
1492137b0e3456d90a79617c1cd5ead5c71b1cfaae9ee41c75d56cd25f404ec73a690f95ce0d8c064c0a14206daca8070aa769b7cdfa904a338a425b52c293fa  fix-default-stacksize.patch
56db8f43afc94713ac65b174189e2dd903b5e1eff0b65f1bdac159e52ad4de6606c449865d73bd47bffad6a8fc339777e2b11395596e9a476867d94a219c7925  fix-elf-native-class.patch
026d20fa1a0f1e27150b833664300250386d7e0d73c0778f81f70242e93e8a16e5607716693bbcdd1efb328fa84c7284e2c6c7e1ac92259b97a9d402975cf709  fix-processinfo_linux.patch
aac12cffc452f1dc365c65944a015476c2011b0975144879d28938c690fe6e77b6bd672e040b4c04c02cb002224e24d6f13adb083324f424ef4cdb79a3a71f6b  fix-resolv.patch
94078abfa74583afef6b5c1f0b334b257cfe87b0db7c13309a9c63d915913d5237c776dbb52f6a23e9409ec390d29e2f7225e9b8c8c5efcbc35b015c613f600c  fix-strerror_r.patch
ecbe6cb579b33dd4888096712f150772db06fd38219ca2a7679b1dc1ee73b0c3f5ee498af12ecd0265b5231a9fe6b7c12b2c1d606ed04caa6aa00c3ad3fe925a  wiredtiger.patch
9bcd870742c31bf25f34188ddc3c414de1103e9860dea9f54eee276b89bc2cf1226abab1749c5cda6a6fb0880e541373754e5e83d63cc7189d4b9c274fd555c3  mongodb.confd
74009794d566dd9d70ec93ffd95c830ee4696165574ecf87398165637fb40799b38d182ef54c50fd0772d589be94ade7f7a49247f3d31c1f012cb4e44cc9f5df  mongodb.initd
8c089b1a11f494e4148fb4646265964c925bf937633a65e395ee1361d42facf837871dd493a9a2e0f480ae0e0829dbd3ed60794c5334e2716332e131fc5c2c51  mongodb.logrotate
61d8734cef644187eeadc821c89e63a3fbf61860fe2db6e74557b1c6760fe83ba7549cb04f9e3aacea4d8e7e4d81a3b1bc0d5e29715eca33c4761adb17ea9ab7  mongos.confd
13aad7247b848ac58b2bc0b40a0d30d909e950380abd0c83fab0e2a394a42dc268a66dac53cf9feec6971739977470082cc4339cca827f41044cfe43803ef3f7  mongos.initd"
