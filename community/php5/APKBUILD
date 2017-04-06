# Contributor: Andy Blyler <andy@blyler.cc>
# Contributor: Łukasz Jendrysik <scadu@yandex.com>
# Contributor: Carlo Landmeter <clandmeter@gmail.com>
# Maintainer: Matt Smith <mcs@darkregion.net>
pkgname=php5
pkgver=5.6.30
pkgrel=0
pkgdesc="The PHP language runtime engine"
url="http://www.php.net/"
arch="all"
license="PHP-3"
depends="$pkgname-cli"
depends_dev="$pkgname-cli pcre-dev"
install="$pkgname.post-upgrade"
provides="php"
makedepends="
	$depends_dev
	apache2-dev
	apr-dev
	apr-util-dev
	aspell-dev
	bzip2-dev
	curl-dev
	db-dev
	enchant-dev
	expat-dev
	freetds-dev
	freetype-dev
	gdbm-dev
	gettext-dev
	gmp-dev
	icu-dev
	imap-dev
	libevent-dev
	libgcrypt-dev
	libjpeg-turbo-dev
	libmcrypt-dev
	libpng-dev
	libtool
	libxml2-dev
	libxslt-dev
	mariadb-dev
	net-snmp-dev
	openldap-dev
	libressl-dev
	postgresql-dev
	readline-dev
	sqlite-dev
	unixodbc-dev
	zlib-dev
	autoconf
	bison
	"
subpackages="$pkgname-dbg $pkgname-dev $pkgname-doc $pkgname-common::noarch $pkgname-cgi
	$pkgname-cli $pkgname-fpm $pkgname-apache2 $pkgname-embed
	$pkgname-phpdbg

	$pkgname-pear::noarch
	$pkgname-bcmath
	$pkgname-bz2
	$pkgname-calendar
	$pkgname-ctype
	$pkgname-curl:_curl
	$pkgname-dba
	$pkgname-dom
	$pkgname-enchant
	$pkgname-exif
	$pkgname-ftp
	$pkgname-gd
	$pkgname-gettext
	$pkgname-gmp
	$pkgname-iconv
	$pkgname-imap
	$pkgname-intl
	$pkgname-json
	$pkgname-ldap
	$pkgname-mcrypt
	$pkgname-mysql
	$pkgname-mysqli
	$pkgname-odbc
	$pkgname-openssl
	$pkgname-pcntl
	$pkgname-pdo
	$pkgname-pdo_mysql
	$pkgname-pdo_odbc
	$pkgname-pdo_pgsql
	$pkgname-pdo_sqlite
	$pkgname-pgsql
	$pkgname-phar
	$pkgname-posix
	$pkgname-pspell
	$pkgname-shmop
	$pkgname-snmp
	$pkgname-soap
	$pkgname-sockets
	$pkgname-sqlite3
	$pkgname-sysvmsg
	$pkgname-sysvsem
	$pkgname-sysvshm
	$pkgname-xml
	$pkgname-xmlreader
	$pkgname-xmlrpc
	$pkgname-xsl
	$pkgname-zip
	$pkgname-zlib
	$pkgname-mssql
	$pkgname-pdo_dblib
	$pkgname-wddx
	$pkgname-opcache
	"

source="http://php.net/distributions/php-$pkgver.tar.bz2
	php-fpm.initd
	php5-module.conf
	php-install-pear-xml.patch
	gd-iconv.patch
	"

_apiver="20131106"
_extdir="/usr/lib/$pkgname/modules"
_srcdir="$srcdir"/php-$pkgver
_confdir=/etc/$pkgname

# seems like pear hardcode /usr/share/pear directory
_peardir=/usr/share/pear

prepare() {
	cd "$_srcdir"
	update_config_sub
	for i in $source; do
		case $i in
		*.patch) msg $i; patch -p1 -i "$srcdir"/$i || return 1;;
		esac
	done

	# safty check for api changes
	local vapi=$(sed -n '/#define PHP_API_VERSION/{s/.* //;p}' main/php.h)
	if [ "$vapi" != "$_apiver" ]; then
		error "Upstreram API version is now $vapi. Expecting $_vapi"
		return 1
	fi
	autoconf
}

_do_build() {
	local _flavor="$1"
	shift
	local _builddir="$srcdir"/build-$_flavor
	mkdir -p "$_builddir"
	cd "$_builddir"
	export EXTENSION_DIR=$_extdir
	export PEAR_INSTALLDIR="$_peardir"
	"$_srcdir"/configure $@ || return 1
	sed -ri "s/^(EXTRA_LDFLAGS[ ]*\=.*)/\1 -lpthread/" Makefile  # see #183
	make || return 1
}

build() {
	_phpconfig="\
		--build=$CBUILD \
		--host=$CHOST \
		--prefix=/usr \
		--sysconfdir=$_confdir \
		--localstatedir=/var \
		--with-layout=GNU \
		--with-config-file-path=$_confdir \
		--with-config-file-scan-dir=$_confdir/conf.d \
		--enable-inline-optimization \
		--disable-debug \
		--disable-rpath \
		--disable-static \
		--enable-shared \
		--mandir=/usr/share/man \
		--with-pic \
		"

	_phpextensions=" \
		--enable-bcmath=shared \
		  --with-bz2=shared \
		--enable-calendar=shared \
		  --with-cdb \
		--enable-ctype=shared \
		  --with-curl=shared \
		--enable-dba=shared \
		  --with-db4=shared \
		--enable-dom=shared \
		  --with-enchant=shared \
		--enable-exif=shared \
		  --with-freetype-dir=shared,/usr \
		--enable-ftp=shared \
		  --with-gd=shared \
		--enable-gd-native-ttf \
		  --with-gdbm=shared \
		  --with-gettext=shared \
		  --with-gmp=shared \
		  --with-iconv=shared \
		  --with-icu-dir=/usr \
		  --with-imap=shared \
		  --with-imap-ssl=shared \
		--enable-intl=shared \
		  --with-jpeg-dir=shared,/usr \
		--enable-json=shared \
		  --with-ldap=shared \
		--enable-libxml=shared \
		--enable-mbregex \
		--enable-mbstring=all \
		  --with-mcrypt=shared \
		  --with-mysql=shared,mysqlnd \
		  --with-mysql-sock=/var/run/mysqld/mysqld.sock \
		  --with-mysqli=shared,mysqlnd \
		  --with-openssl=shared \
		  --with-pcre-regex=/usr \
		--enable-pcntl=shared \
		--enable-pdo=shared \
		  --with-pdo-mysql=shared,mysqlnd \
		  --with-pdo-odbc=shared,unixODBC,/usr \
		  --with-pdo-pgsql=shared \
		  --with-pdo-sqlite=shared,/usr \
		  --with-pgsql=shared \
		--enable-phar=shared \
		  --with-png-dir=shared,/usr \
		--enable-posix=shared \
		  --with-pspell=shared \
		  --with-regex=php \
		--enable-session \
		--enable-shmop=shared \
		  --with-snmp=shared \
		--enable-soap=shared \
		--enable-sockets=shared \
		  --with-sqlite3=shared,/usr \
		--enable-sysvmsg=shared \
		--enable-sysvsem=shared \
		--enable-sysvshm=shared \
		  --with-unixODBC=shared,/usr \
		--enable-xml=shared \
		--enable-xmlreader=shared \
		  --with-xmlrpc=shared \
		  --with-xsl=shared \
		--enable-wddx=shared \
		--enable-zip=shared \
		  --with-zlib=shared \
		--without-db1 \
		--without-db2 \
		--without-db3 \
		--without-qdbm \
		--with-mssql=shared \
		--with-pdo-dblib=shared \
		--enable-opcache \
		"

	# cgi, fcgi, cli, pear and extensions
	_do_build cgi \
		${_phpconfig} \
		--disable-cli \
		--enable-cgi \
		--enable-cli \
		--with-pear \
		--with-readline \
		--enable-phpdbg \
		${_phpextensions} \
		|| return 1

	# fpm
	cp -a "$srcdir"/build-cgi "$srcdir"/build-fpm
	_do_build fpm \
		${_phpconfig} \
		--disable-cli \
		--enable-fpm \
		${_phpextensions} \
		|| return 1

	# apache2
	cp -a "$srcdir"/build-cgi "$srcdir"/build-apache2
	_do_build apache2 \
		${_phpconfig} \
		--disable-cli \
		--with-apxs2 \
		${_phpextensions} \
		|| return 1

	# embed
	cp -a "$srcdir"/build-cgi "$srcdir"/build-embed
	_do_build embed \
		${_phpconfig} \
		--disable-cli \
		--enable-embed=shared \
		${_phpextensions} \
		|| return 1
}

package() {
	cd "$srcdir"/build-cgi
	# install php-cgi, cli, pear and modules
	make -j1 install install-pear INSTALL_ROOT="$pkgdir" || return 1

	# cleanup after pear
	find "$pkgdir" -name '.*' | xargs rm -rf || return 1

	# install embed
	install -D -m755 "$srcdir"/build-embed/libs/libphp5.so \
		"$pkgdir"/usr/lib/libphp5.so || return 1
	install -D -m644 "$_srcdir"/sapi/embed/php_embed.h \
		"$pkgdir"/usr/include/php/sapi/embed/php_embed.h || return 1

	# Create symlink with version suffix, parallel to php7 package,
	# to simplify multiversion aports and prepare us for future.
	local f; for f in php php-cgi php-config phpdbg phpize; do
		ln -s $f "$pkgdir"/usr/bin/${f}5 || return 1
	done
}

dev() {
	default_dev || return 1

	mkdir -p "$subpkgdir"/usr/lib/php
	mv "$pkgdir"/usr/lib/php/build \
		"$subpkgdir"/usr/lib/php/ || return 1
	mv "$pkgdir"/usr/bin/php-config5 "$subpkgdir"/usr/bin/
}

doc() {
	# man pages
	default_doc || return 1
	cd "$srcdir"/php-$pkgver

	# doc files
	_docs="CODING_STANDARDS CREDITS EXTENSIONS INSTALL LICENSE NEWS \
	UPGRADING UPGRADING.INTERNALS"
	for _doc in $_docs README.*; do
		install -Dm644 "$srcdir"/php-$pkgver/$_doc \
			"$subpkgdir"/usr/share/doc/$pkgname/$_doc || return 1
	done
}

common() {
	pkgdesc="PHP Common Files"
	depends=""

	cd "$srcdir"/php-$pkgver

	install -D -m644 php.ini-production "$subpkgdir"$_confdir/php.ini
	sed -ri -e "s:^; extension_dir = \"./\":extension_dir = \"$_extdir\":" \
		-e 's/;(date.timezone =)/\1 UTC/' \
		-e "s~^([;]*cgi\.rfc2616_headers.*)$~\1\n\n\; If this is enabled, the PHP CGI binary can safely be placed outside of the\n; web tree and people will not be able to circumvent .htaccess security.\ncgi\.discard_path = 1~" \
		"$subpkgdir"$_confdir/php.ini
}

cgi() {
	pkgdesc="PHP Common Gateway Interface (CGI)"
	depends="$pkgname-common"
	mkdir -p "$subpkgdir"/usr/bin
	mv "$pkgdir"/usr/bin/php-cgi* "$subpkgdir"/usr/bin/
}

cli() {
	pkgdesc="PHP Command Line Interface (CLI)"
	depends="$pkgname-common"
	mkdir -p "$subpkgdir"/usr/bin
	mv "$pkgdir"/usr/bin/php "$subpkgdir"/usr/bin/ || return 1
	mv "$pkgdir"/usr/bin/php5 "$subpkgdir"/usr/bin/ || return 1
	# provide phpize here instead of -dev due to pecl command
	mv "$pkgdir"/usr/bin/phpize* "$subpkgdir"/usr/bin/ || return 1
}

fpm() {
	pkgdesc="PHP FastCGI Process Manager (FPM)"
	depends="$pkgname-common"
	mkdir -p "$subpkgdir"$_confdir/fpm.d
	install -D -m755 "$srcdir"/build-fpm/sapi/fpm/php-fpm \
		"$subpkgdir"/usr/bin/php-fpm || return 1
	install -D -m644 "$srcdir"/build-fpm/sapi/fpm/php-fpm.conf \
		"$subpkgdir"$_confdir/php-fpm.conf || return 1
	install -D -m755 "$srcdir"/php-fpm.initd "$subpkgdir"/etc/init.d/php-fpm
	# enable some default options
	sed -ri	-e "s~^;(error_log)(.*)~\1 = /var/log/php-fpm.log~" \
		-e "s~^;(include)(.*)~\1 = $_confdir/fpm.d/*.conf~" \
		-e "s/^;(pm.start_servers)/\1/" \
		-e "s/^;(pm.min_spare_servers)/\1/" \
		-e "s/^;(pm.max_spare_servers)/\1/" \
		"$subpkgdir"$_confdir/php-fpm.conf || return 1
	ln -s php-fpm "$subpkgdir"/usr/bin/php-fpm5
}

apache2() {
	pkgdesc="PHP Module for Apache2"
	depends="$pkgname-common apache2"
	install -D -m755 "$srcdir"/build-apache2/libs/libphp5.so \
		"$subpkgdir"/usr/lib/apache2/libphp5.so || return 1
	install -D -m644 "$srcdir"/php5-module.conf \
		"$subpkgdir"/etc/apache2/conf.d/php5-module.conf || return 1
}

embed() {
	pkgdesc="PHP Embed Library"
	depends="$pkgname-common"
	mkdir -p "$subpkgdir"/usr/lib
	mv "$pkgdir"/usr/lib/libphp5.so "$subpkgdir"/usr/lib/
}

pear() {
	pkgdesc="PHP Extension and Application Repository (PEAR)"
	depends="$pkgname-cli $pkgname-xml"
	mkdir -p "$subpkgdir"/usr/share "$subpkgdir"$_confdir \
		"$subpkgdir"/usr/bin
	mv "$pkgdir"/usr/bin/pecl \
		"$pkgdir"/usr/bin/pear \
		"$pkgdir"/usr/bin/peardev \
		"$subpkgdir"/usr/bin/ || return 1
	mv "$pkgdir"$_confdir/pear.conf \
		"$subpkgdir"$_confdir/ || return 1
	mv "$pkgdir"${_peardir} \
		"$subpkgdir"/usr/share/ || return 1
}

phpdbg() {
	pkgdesc="Interactive PHP debugger"
	mkdir -p "$subpkgdir"/usr/bin
	mv "$pkgdir"/usr/bin/phpdbg* "$subpkgdir"/usr/bin/
}

_mv_ext() {
	local ext=$1
	local ini=$ext.ini
	pkgdesc="${ext} extension for PHP"

	# extension dependencies
	if [ -n "${2-}" ]; then
		depends="${2-}"
	fi
	depends="${pkgname} ${depends}"

	# work around dependency issue
	# https://bugs.alpinelinux.org/issues/1848
	if [ "$ext" = "wddx" ]; then
		ini=xml_$ext.ini
	fi

	mkdir -p "$subpkgdir"/$_extdir
	mv "$pkgdir"/$_extdir/${ext}.so "$subpkgdir"/$_extdir/ || return 1
	mkdir -p "$subpkgdir"$_confdir/conf.d
	case "$1" in
		opcache)
			echo "zend_extension=${ext}.so" > "$subpkgdir"$_confdir/conf.d/$ini
			;;
		*)
			echo "extension=${ext}.so" > "$subpkgdir"$_confdir/conf.d/$ini
			;;
	esac
}

bcmath()	{ _mv_ext bcmath; }
bz2()		{ _mv_ext bz2; }
calendar()	{ _mv_ext calendar; }
ctype()		{ _mv_ext ctype; }
_curl()		{ _mv_ext curl; }
dba()		{ _mv_ext dba; }
dom()		{ _mv_ext dom; }
enchant()	{ _mv_ext enchant; }
exif()		{ _mv_ext exif; }
ftp()		{ _mv_ext ftp; }
gd()		{ _mv_ext gd; }
gettext()	{ _mv_ext gettext; }
gmp()		{ _mv_ext gmp; }
iconv()		{ _mv_ext iconv; }
imap()		{ _mv_ext imap; }
intl()		{ _mv_ext intl; }
json()		{ _mv_ext json; }
ldap()		{ _mv_ext ldap; }
mcrypt()	{ _mv_ext mcrypt; }
mysql()		{ _mv_ext mysql; }
mysqli()	{ _mv_ext mysqli; }
odbc()		{ _mv_ext odbc unixodbc; }
openssl()	{ _mv_ext openssl; }
pcntl()		{ _mv_ext pcntl; }
pdo()		{ _mv_ext pdo; }
pdo_mysql()	{ _mv_ext pdo_mysql $pkgname-pdo; }
pdo_odbc()	{ _mv_ext pdo_odbc $pkgname-pdo; }
pdo_pgsql()	{ _mv_ext pdo_pgsql $pkgname-pdo; }
pdo_sqlite()	{ _mv_ext pdo_sqlite $pkgname-pdo; }
pgsql()		{ _mv_ext pgsql; }
phar()		{
			_mv_ext phar
			mkdir -p "$subpkgdir"/usr/bin
			mv "$pkgdir"/usr/bin/phar* "$subpkgdir"/usr/bin/
		}
posix()		{ _mv_ext posix; }
pspell()	{ _mv_ext pspell; }
shmop()		{ _mv_ext shmop; }
snmp()		{ _mv_ext snmp; }
soap()		{ _mv_ext soap; }
sockets()	{ _mv_ext sockets; }
sqlite3()	{ _mv_ext sqlite3; }
sysvmsg()	{ _mv_ext sysvmsg; }
sysvsem()	{ _mv_ext sysvsem; }
sysvshm()	{ _mv_ext sysvshm; }
xml()		{ _mv_ext xml; }
xmlreader()	{ _mv_ext xmlreader $pkgname-dom; }
xmlrpc()	{ _mv_ext xmlrpc $pkgname-xml; }
xsl()		{ _mv_ext xsl $pkgname-dom; }
zip()		{ _mv_ext zip; }
zlib()		{ _mv_ext zlib; }
mssql()		{ _mv_ext mssql; }
pdo_dblib()	{ _mv_ext pdo_dblib "$pkgname-pdo freetds"; }
wddx()		{ _mv_ext wddx; }
opcache()	{ _mv_ext opcache; }

md5sums="67566191957b5fcac8567a5a9bbdced7  php-5.6.30.tar.bz2
63b16caff0d7aa881a31a1e02f3080c3  php-fpm.initd
67719f428f44ec004da18705cbabe2ee  php5-module.conf
483bc0a85c50a9a9aedbe14a19ed4526  php-install-pear-xml.patch
7200972a23adae799921c4ca20ff0074  gd-iconv.patch"
sha256sums="a105c293fa1dbff118b5b0ca74029e6c461f8c78f49b337a2a98be9e32c27906  php-5.6.30.tar.bz2
be9bfdab10a994fe553119b181be7015325a7618de454a58bdee06bcfb711454  php-fpm.initd
ceec4d5b2a128c6a97e49830af604f0bb555bca1a86a9cd0366b828ba392257f  php5-module.conf
f739ca427a1dd53a388bad0823565299c5d4a5796b1171b892884e4d7d099bab  php-install-pear-xml.patch
98de37c650a36870a543225f6a6b81813ccd447a484f0881511be4eb6e901844  gd-iconv.patch"
sha512sums="12734d786cca5767b8b8838affbe1c3d578dd179c8d5339653d905658562c5fdf39a88349213b1340f320320700a5378aed617447b6e15909019788a49ad2da0  php-5.6.30.tar.bz2
1f5cb18f85a2e279e24344d993f5c51c7bfbcbecc0e9bfcf075bebd1b0b893e2ffb793d95a632c9333033597d4b4f74840bfd00520a6dc700444d1a054225da1  php-fpm.initd
895e94c791bd82060ad820fef049d366a09c932097faa6b7b9a2c2e9e00a18cb7c0f9b128679c7659b404379266fd0f95dba5c0333f626194cf60f7bf6044102  php5-module.conf
f1177cbf6b1f44402f421c3d317aab1a2a40d0b1209c11519c1158df337c8945f3a313d689c939768584f3e4edbe52e8bd6103fb6777462326a9d94e8ab1f505  php-install-pear-xml.patch
6ecd0be2da1dc5b1d7512e46a2a5cd107a8b2a8c364efc9c624a7d6b2ab081685a329c94c22c970dc14c5c1115f702c512e97ae858da1bc69c6423323dbeeba2  gd-iconv.patch"
