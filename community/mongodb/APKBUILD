# Maintainer: Filipp Andronov <filipp.andronov@gmail.com>
# Contributor: Marc Vertes <marc.vertes@ugrid.net>
pkgname=mongodb
pkgver=3.4.9
pkgrel=0
pkgdesc='A high-performance, open source, schema-free document-oriented database'
url='http://www.mongodb.org'
arch='x86_64'
license='AGPL3'
pkgusers="mongodb"
pkggroups="mongodb"
depends=
makedepends="scons paxmark libressl-dev pcre-dev snappy-dev boost-dev asio-dev
	libpcap-dev wiredtiger-dev zlib-dev cyrus-sasl-dev yaml-cpp-dev"
install="$pkgname.pre-install"
source="http://downloads.mongodb.org/src/mongodb-src-r${pkgver}.tar.gz
	40-fix-elf-native-class.patch
	backtrace.patch
	fix-asio-strerror_r.patch
	fix-libressl.patch
	fix-log.patch
	fix-processinfo_linux.patch
	fix-std-string.patch
	set-default-stacksize.patch
	wiredtiger-strtouq.patch
	boost160.patch
	boost162.patch

        mongodb.confd
        mongodb.initd
        mongodb.logrotate
        mongos.confd
        mongos.initd
	"
#
# 1. Force 64bits because of 40-fix-elf-native-class.patch
# 2. Use system allocator as tc-malloc doesn't build
# 2. Use as much system libs as possible, boost doesn't compile foe example
#
# TODO: proper elf-netive-class fix
# TODO: check if there are more libs could be replaced by system counterparts (see libpcap does't use, for example)
# TODO: proper fix for heap usage
# Right now code patched to always return 0 for heap usage statistics. That is necessary because musl malloc info
# isn't compatible with glibc and breaks build. It is _possible_ to patch code to return 0
# because on 64bit platform statistics is broken and returns wrong numbers, see SERVER-9168 mongo bug.
#
# There is a way to return propper value when tc-malloc used, but it doesn't compile for libmusl. Work is in progress,
# contribution strongly welcome :D
#
_builddir="$srcdir"/$pkgname-src-r$pkgver
_buildopts="
		--allocator=system \
		--disable-warnings-as-errors \
		--use-system-boost \
		--use-system-pcre \
		--use-system-wiredtiger \
		--use-system-snappy \
		--use-system-zlib \
		--use-system-yaml \
		--use-sasl-client \
		--ssl \
	"

prepare() {
        cd "$_builddir"

        local i
        for i in $source; do
                case $i in
                *.patch) msg $i; patch -p1 -i "$srcdir"/$i || return 1;;
                esac
        done
}

build() {
        cd "$_builddir"

	export SCONSFLAGS="$MAKEFLAGS"
	scons $_buildopts --prefix=$pkgdir/usr core
}

package() {
        cd "$_builddir"

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

sha512sums="b6803c91e9cda8e6963359386d2014d03f68151f64d580d5baacc3c66b2adabc62ee5c2cf203b9aee7d11942934afc6f9e17364d2f3aafd238ba88d13c77f26d  mongodb-src-r3.4.9.tar.gz
56db8f43afc94713ac65b174189e2dd903b5e1eff0b65f1bdac159e52ad4de6606c449865d73bd47bffad6a8fc339777e2b11395596e9a476867d94a219c7925  40-fix-elf-native-class.patch
7d097f497cb910c9cb81086cd8c222e43456d1a6de4adfe3e97a4d99add454279350fdeb7305dab84b3deca04afd824036d4065ee0fb8cdd8c03e96d98ee86af  backtrace.patch
f829b1ad542db3ee776d444243b8b47ab4e48a7386d9b199d7b1adafd31556cf173a5683b78ee735d6a69999ad9af5ad152fde955bbe8865f7910718991ce97c  fix-asio-strerror_r.patch
8f2832f10e47b1a9f413ab44eb2b75dbb7bc47282d3ba721f35d4a93bc4fcf18b88c5f1c2f0ccf28539bbf81ee4c5715c5b71506fa680d22cde0630f9b3e2d22  fix-libressl.patch
9e109a9131e8466496e94f7046e13fd40ec750c8de703e31d65cf3f6e80830e67e1438debaefc0e1150fe2bb08dbb42cf95890c51e9d98f354cfbb396500b5d4  fix-log.patch
026d20fa1a0f1e27150b833664300250386d7e0d73c0778f81f70242e93e8a16e5607716693bbcdd1efb328fa84c7284e2c6c7e1ac92259b97a9d402975cf709  fix-processinfo_linux.patch
de2523a2c0e3b2d56158ff697e69e3e5e1d65cb29e8a0f07a3a2794f6c4ba8abfe62d1871eb72c823f17399327f4741975a6424f011c95031e60e309d267ccd0  fix-std-string.patch
1492137b0e3456d90a79617c1cd5ead5c71b1cfaae9ee41c75d56cd25f404ec73a690f95ce0d8c064c0a14206daca8070aa769b7cdfa904a338a425b52c293fa  set-default-stacksize.patch
bbb323d428d59584703e8692bf4df7fe0d37c0324c23822bade2edd1ca78759191f778230b7107502a9d2f7f22afc84d4ea350139fc1d751ceb2fff219b9ddf8  wiredtiger-strtouq.patch
385c82875174caae433a3b381eb10f98a6fed0c8943788ddefff1de80a898e480bdbbf094a7783285cf2ae11ce3fc6878e57d58183d05be2f0837b206aaa4da6  boost160.patch
79edfd1a6eaba597b31a82e54722dccab288d8b8840a53f79140b5fca221b5acd9fbc770d99e46ea9fa0da502cdf18dd35d982c95a4aa341806c3d8b61fc732f  boost162.patch
9bcd870742c31bf25f34188ddc3c414de1103e9860dea9f54eee276b89bc2cf1226abab1749c5cda6a6fb0880e541373754e5e83d63cc7189d4b9c274fd555c3  mongodb.confd
74009794d566dd9d70ec93ffd95c830ee4696165574ecf87398165637fb40799b38d182ef54c50fd0772d589be94ade7f7a49247f3d31c1f012cb4e44cc9f5df  mongodb.initd
8c089b1a11f494e4148fb4646265964c925bf937633a65e395ee1361d42facf837871dd493a9a2e0f480ae0e0829dbd3ed60794c5334e2716332e131fc5c2c51  mongodb.logrotate
61d8734cef644187eeadc821c89e63a3fbf61860fe2db6e74557b1c6760fe83ba7549cb04f9e3aacea4d8e7e4d81a3b1bc0d5e29715eca33c4761adb17ea9ab7  mongos.confd
13aad7247b848ac58b2bc0b40a0d30d909e950380abd0c83fab0e2a394a42dc268a66dac53cf9feec6971739977470082cc4339cca827f41044cfe43803ef3f7  mongos.initd"
