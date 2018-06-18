# Contributor: Jakub Jirutka <jakub@jirutka.cz>
# Contributor: Shiz <hi@shiz.me>
# Contributor: Jeizsm <jeizsm@gmail.com>
# Maintainer: Jakub Jirutka <jakub@jirutka.cz>
pkgname=rust
pkgver=1.26.2
# TODO: bump to 6 as soon as we add llvm6
_llvmver=5
_bootver=1.25.0
pkgrel=1
pkgdesc="The Rust Programming Language"
url="http://www.rust-lang.org"
arch="x86_64 ppc64le"
license="Apache-2.0 BSD ISC MIT"

# gcc is needed at runtime just for linking. Someday rustc might invoke
# the linker directly, and then we'll only need binutils.
# See: https://github.com/rust-lang/rust/issues/11937
depends="$pkgname-stdlib=$pkgver-r$pkgrel gcc llvm-libunwind-dev musl-dev"

# * Rust is self-hosted, so you need rustc (and cargo) to build rustc...
#   The last revision of this abuild that does not depend on itself (uses
#   prebuilt rustc and cargo) is 8cb3112594f10a8cee5b5412c28a846acb63167f.
# * libffi-dev is needed just because we compile llvm with LLVM_ENABLE_FFI.
makedepends="
	rust-bootstrap>=$_bootver
	cargo-bootstrap
	cmake
	curl-dev
	file
	libffi-dev
	libgit2-dev
	libressl-dev
	libssh2-dev
	llvm$_llvmver-dev
	llvm$_llvmver-test-utils
	python2
	tar
	zlib-dev
	util-linux
	"
# XXX: This is a hack to allow this abuild to depend on itself. Adding "rust"
# to makedepends would not work, because abuild implicitly removes $pkgname
# and $subpackages from the abuild's dependencies.
provides="rust-bootstrap=$pkgver-r$pkgrel"
# This is needed for -src that contains some testing binaries.
options="!archcheck"
subpackages="
	$pkgname-dbg
	$pkgname-stdlib
	$pkgname-analysis
	$pkgname-gdb::noarch
	$pkgname-lldb::noarch
	$pkgname-doc
	$pkgname-src::noarch
	cargo
	cargo-bash-completions:_cargo_bashcomp:noarch
	cargo-zsh-completion:_cargo_zshcomp:noarch
	cargo-doc:_cargo_doc:noarch
	"
source="https://static.rust-lang.org/dist/rustc-$pkgver-src.tar.gz
	musl-fix-static-linking.patch
	musl-fix-linux_musl_base.patch
	llvm-with-ffi.patch
	static-pie.patch
	need-rpath.patch
	minimize-rpath.patch
	alpine-move-py-scripts-to-share.patch
	alpine-change-rpath-to-rustlib.patch
	alpine-target.patch
	install-template-shebang.patch
	fix-configure-tools.patch
	bootstrap-tool-respect-tool-config.patch
	ensure-stage0-libs-have-unique-metadata.patch
	cargo-libressl27x.patch
	cargo-tests-fix-build-auth-http_auth_offered.patch
	cargo-tests-ignore-resolving_minimum_version_with_transitive_deps.patch
	cargo_libc_1.26.2_b32modrs.patch
	cargo_libc_1.26.2_b64modrs.patch
	cargo_libc_1.26.2_b64powerpc64.patch
	cargo_libc_1.26.2_b64x86_64.patch
	cargo_libc_1.26.2_modrs.patch
	cargo_libc_1.26.2_checksum.patch
	ppc64le_target.patch
	rust_libc_1.26.2_modrs.patch
	rust_libc_1.26.2_b64x86_64.patch
	rust_libc_1.26.2_b64powerpc64.patch
	rust_libc_1.26.2_b64modrs.patch
	rust_libc_1.26.2_b32modrs.patch
	check-rustc
	"
builddir="$srcdir/rustc-$pkgver-src"

_rlibdir="usr/lib/rustlib/$CTARGET/lib"
_sharedir="usr/share/rust"

ldpath="/$_rlibdir"

export RUST_BACKTRACE=1
export RUSTC_CRT_STATIC="false"
# Convince libgit2-sys to use the distro libgit2.
export LIBGIT2_SYS_USE_PKG_CONFIG=1

prepare() {
	default_prepare

	cd "$builddir"

	# Remove bundled dependencies.
	rm -Rf src/llvm/
}

build() {
	local parallel_opts=""
	if [ "$CARCH" = "ppc64le" ]; then
		parallel_opts="taskset 0x1"
	fi
	cd "$builddir"

	# jemalloc is disabled, because it increases size of statically linked
	# binaries produced by rustc (stripped hello_world 186 kiB vs. 358 kiB)
	# for only tiny performance boost (even negative in some tests).
	./configure \
		--build="$CBUILD" \
		--host="$CTARGET" \
		--target="$CTARGET" \
		--prefix="/usr" \
		--release-channel="stable" \
		--enable-local-rust \
		--local-rust-root="/usr" \
		--llvm-root="/usr/lib/llvm$_llvmver" \
		--musl-root="/usr" \
		--disable-docs \
		--enable-extended \
		--tools="analysis,cargo,src" \
		--enable-llvm-link-shared \
		--enable-option-checking \
		--enable-locked-deps \
		--enable-vendor \
		--disable-jemalloc

	$parallel_opts ./x.py build -v --jobs ${JOBS:-2}
}

check() {
	cd "$builddir"

	# At this moment lib/rustlib/$CTARGET/lib does not contain a complete
	# copy of the .so libs from lib (they will be copied there during
	# `x.py install`). Thus we must set LD_LIBRARY_PATH for tests to work.
	# This is related to change-rpath-to-rustlib.patch.
	if [ "$CARCH" != "ppc64le" ]; then
		export LD_LIBRARY_PATH="$builddir/build/$CTARGET/stage2/lib"

		"$srcdir"/check-rustc "$builddir"/build/$CTARGET/stage2/bin/rustc

# XXX: There's some problem with these tests, we will figure it out later.
#	cd "$builddir"
#	make check \
#		LD_LIBRARY_PATH="$_stage0dir/lib" \
#		VERBOSE=1

		msg "Running tests for cargo..."
		CFG_DISABLE_CROSS_TESTS=1 ./x.py test --no-fail-fast src/tools/cargo

		unset LD_LIBRARY_PATH
	fi
}

package() {
	cd "$builddir"
	local parallel_opts=""
	if [ "$CARCH" = "ppc64le" ]; then
		parallel_opts="taskset 0x1"
	fi


	DESTDIR="$pkgdir" $parallel_opts ./x.py install -v

	cd "$pkgdir"

	# These libraries are identical to those under rustlib/. Since we have
	# linked rustc/rustdoc against those under rustlib/, we can remove
	# them. Read change-rpath-to-rustlib.patch for more info.
	rm -r usr/lib/*.so

	# These objects are for static linking with musl on non-musl systems.
	rm $_rlibdir/crt*.o

	# Shared objects should have executable flag.
	chmod +x $_rlibdir/*.so

	# Python scripts are noarch, so move them to /usr/share.
	# Requires move-py-scripts-to-share.patch to be applied.
	_mv usr/lib/rustlib/etc/*.py $_sharedir/etc/
	rmdir -p usr/lib/rustlib/etc 2>/dev/null || true

	# Remove some clutter.
	cd usr/lib/rustlib
	rm components install.log manifest-* rust-installer-version uninstall.sh
}

stdlib() {
	pkgdesc="Standard library for Rust (static rlibs)"

	_mv "$pkgdir"/$_rlibdir/*.rlib "$subpkgdir"/$_rlibdir/
}

analysis() {
	pkgdesc="Compiler analysis data for the Rust standard library"
	depends="$pkgname-stdlib=$pkgver-r$pkgrel"

	_mv "$pkgdir"/$_rlibdir/../analysis "$subpkgdir"/${_rlibdir%/*}/
}

gdb() {
	pkgdesc="GDB pretty printers for Rust"
	depends="$pkgname gdb"

	mkdir -p "$subpkgdir"
	cd "$subpkgdir"

	_mv "$pkgdir"/usr/bin/rust-gdb usr/bin/
	_mv "$pkgdir"/$_sharedir/etc/gdb_*.py $_sharedir/etc/
}

lldb() {
	pkgdesc="LLDB pretty printers for Rust"
	depends="$pkgname lldb py2-lldb"

	mkdir -p "$subpkgdir"
	cd "$subpkgdir"

	_mv "$pkgdir"/usr/bin/rust-lldb usr/bin/
	_mv "$pkgdir"/$_sharedir/etc/lldb_*.py $_sharedir/etc/
}

src() {
	pkgdesc="$pkgdesc (source code)"
	depends="$pkgname"
	license="$license OFL-1.1 GPL-3.0-or-later GPL-3.0-with-GCC-exception CC-BY-SA-3.0 LGPL-3.0"

	_mv "$pkgdir"/usr/lib/rustlib/src/rust "$subpkgdir"/usr/src/
	rmdir -p "$pkgdir"/usr/lib/rustlib/src 2>/dev/null || true

	mkdir -p "$subpkgdir"/usr/lib/rustlib/src
	ln -s ../../../src/rust "$subpkgdir"/usr/lib/rustlib/src/rust
}

cargo() {
	pkgdesc="The Rust package manager"
	license="Apache-2.0 MIT UNLICENSE"
	depends="$pkgname"
	# XXX: See comment on top-level provides=.
	provides="cargo-bootstrap=$pkgver-r$pkgrel"

	_mv "$pkgdir"/usr/bin/cargo "$subpkgdir"/usr/bin/
}

_cargo_bashcomp() {
	pkgdesc="Bash completions for cargo"
	license="Apache-2.0 MIT"
	depends=""
	install_if="cargo=$pkgver-r$pkgrel bash-completion"

	cd "$pkgdir"
	_mv etc/bash_completion.d/cargo \
		"$subpkgdir"/usr/share/bash-completion/completions/
	rmdir -p etc/bash_completion.d 2>/dev/null || true
}

_cargo_zshcomp() {
	pkgdesc="ZSH completions for cargo"
	license="Apache-2.0 MIT"
	depends=""
	install_if="cargo=$pkgver-r$pkgrel zsh"

	cd "$pkgdir"
	_mv usr/share/zsh/site-functions/_cargo \
		"$subpkgdir"/usr/share/zsh/site-functions/_cargo
	rmdir -p usr/share/zsh/site-functions 2>/dev/null || true
}

_cargo_doc() {
	pkgdesc="The Rust package manager (documentation)"
	license="Apache-2.0 MIT"
	install_if="docs cargo=$pkgver-r$pkgrel"

	# XXX: This is hackish!
	cd "$pkgdir"/../$pkgname-doc
	_mv usr/share/man/man1/cargo* "$subpkgdir"/usr/share/man/man1/
}

_mv() {
	local dest; for dest; do true; done  # get last argument
	mkdir -p "$dest"
	mv $@
}

sha512sums="e864d01890d8c9cf5c03f9e8572ee66c8619873f365190cc234ccf19883ea235215e42ab475d0921f82e1842f4e1a413aa43bb23cddb555bc24edb62b2dab9de  rustc-1.26.2-src.tar.gz
d26b0c87e2dce9f2aca561a47ed1ca987c4e1b272ab8b19c39433b63d4be03ca244ba97adc50e743fe50eb0b2f8109cd68a2f05e41d7411c58ef77ef253ca789  musl-fix-static-linking.patch
ed209fa8e44764fce9e38f46006910d632b81708be5d84d281aa40cf4f78fb9f8ccb0dcac55cb4e5a5855a4e52fc322b72772006c5d5d1d4545cf2668e60f381  musl-fix-linux_musl_base.patch
e40d41a6dc5d400d6672f1836cd5b9e00391f7beb52e872d87db76bc95a606ce6aaae737a0256a1e5fba77c83bb223818d214dbe87028d47be65fb43c101595c  llvm-with-ffi.patch
a8ae797e487cb7722b2c88a641ae850d65997d296b1f9672d0ec23caff99846c6f2eaa27eb449fec31c51c3d490aee2900e722c3435fab95ed55a22fda583168  static-pie.patch
7bf81f58935e56ab673ce85e0c81b94cfb78a5bfbb8c220683a4cf71d75dfdf3861300abf3867c46594d2db894d00e5c8f65983a2b9bfe8966582adfa7d149e3  need-rpath.patch
d352614e7c774e181decae210140e789de7fc090327ff371981ad28a11ce51c8c01b27c1101a24bb84d75ed2f706f67868f7dbc52196d4ccdf4ebd2d6d6b6b5e  minimize-rpath.patch
0c0aa7eeddeb578c320a94696a4437fbf083ef4d6f8049512de82548285f37ec4460b5d04f087dc303a5f62a09b5d13b7f0c4fbbdb0b321147ae030e7282ac07  alpine-move-py-scripts-to-share.patch
61aa415d754e9e01236481a1f3c9d5242f2d633e6f11b998e9ffcc07bf5c182d87c0c973dab6f10e4bb3ab4b4a4857bf9ed8dd664c49a65f6175d27db2774db1  alpine-change-rpath-to-rustlib.patch
b3be85bf54d03ba5a685c8e01246e047a169fedb1745182286fdb1ae8cb23e6723318276ef36ee0c54bf7e6d2bc86a46c479fb6c822b8b548d35fa094dde05d2  alpine-target.patch
7d59258d4462eba0207739a5c0c8baf1f19d9a396e5547bb4d59d700eb94d50ba6add2e523f3e94e29e993821018594625ea4ac86304fb58f7f8c82622a26ab0  install-template-shebang.patch
775a7a28a79d4150813caef6b5b1ee0771cf3cb5945eae427371618ff1fb097da9a0001e13f0f426e3a9636f75683bfe4bdff634456137e057f965ee2899b95a  fix-configure-tools.patch
b0f117423f0a9f51c2fecfcc63acabcd7da692946113b6e0aa30f2cff529a06bc41a2b075b410badab6c11fd4e1147b4af796e3e9a93608d3b43ee65b0a4aa02  bootstrap-tool-respect-tool-config.patch
df8caff62724e4d4ced52a72f919c3b41f272b5e547dac5aaccbc70f0cae2edf0002c755275e228944effac82fece63274e8fc717dca171b525fd51865151c75  ensure-stage0-libs-have-unique-metadata.patch
a1c6cc6181d48e313c9c976cb403437cee8d49bda6ef612df7bc21981abc21177b6682ae6b1e4d4906d97ab21f32b310272f57b97ad68ad0f351cd923afeb2f2  cargo-libressl27x.patch
332a6af59edc507baa73eda1de60591dd4202f540541769ac1bcbc731267f4523ea309d2c3b1f5a9dc3db32831942a5d3d40b81882dad0bf0b5ee7f74f1d6477  cargo-tests-fix-build-auth-http_auth_offered.patch
3d6f027088e1ec189ce864bf5ed150ccad8be5d9fc0973f1b4d202eec6eab865834403335a9f0765bbfa54638aed7f5d5f2183ba9dfeab9f5bc4ef48111a8427  cargo-tests-ignore-resolving_minimum_version_with_transitive_deps.patch
f0ce5b73bc32f75a8b7afde8a368f19aa920da5ffba8f65bb5df61de16ccc8214f97bd77efbd49e5fee8486a06d645bcbe12de9cdd415e4c8f3482064c22a0af  cargo_libc_1.26.2_b32modrs.patch
2070e8f9bd58f7754b5f343b73522397ff4861a69673d7ddc78e9a13b14415b969b8495b254ed4b8dccf94cd7ffd5d3558edeb214e04b890f87f219c280139a4  cargo_libc_1.26.2_b64modrs.patch
481386baecf58b240ee3c3ab969fe15427ebaec17628655b3cf63f16a654f073ff8657cd7d7d7c76dd03784f57f62bbd6ff44d6d379ae1ebb100213ba7007a78  cargo_libc_1.26.2_b64powerpc64.patch
1dd94a08d57850355df300e31f32a0fbe3a22ccd0cd260a7fa064fd7d1fdf3d2dfd6811a68fb9a013e057f8996c97de53e2cea1467a71b6dc9ddf1733a2326ae  cargo_libc_1.26.2_b64x86_64.patch
9170654bbfc7c254d115a71058ed4f2388bcee5a200f21635df62b3a50818f55cb709504317d8629e7a01c5c46e2cb5429ba4d6ba453a6bd6d5160526e4d492f  cargo_libc_1.26.2_modrs.patch
254930c669853a357d3d93acbeb097b141512377c9b2113475a20d8251a0618d1421d8ddcdbf3534f3284167d2d0660c5cff065ebfaf0dcbba990c636a934d6d  cargo_libc_1.26.2_checksum.patch
6c13a35cfd7d913fb5c2003ca6489a6cf794432346a8a7fc4a416241d4f1bf1cb6cca8088aad2bcd2a8e41ef4dd912ebc6661608061df4804eb147ae53c7f01a  ppc64le_target.patch
989ffd527940eeab5a75609283673d2681e870aeabfb127d8a50520f15196f774ab86238e20aa722f7624e0137761d69fe3e52f3028271c8bdb769fc3490eac9  rust_libc_1.26.2_modrs.patch
ef1c46711188eb50067998be5757b3d38164925994a19266eb548de36392cdc783b26270dd3482e5649b1c72025e49cdda38e3d8f448f38b247dae3f3b1c8191  rust_libc_1.26.2_b64x86_64.patch
322a7d3a80a1ec94c305429507bc4867e9afdb8bbc761b041ae2adfb7254f0143aab81a17b04f0fa1b1427598ea4972b6f7cabde0bc49dc909c379f52c6d903a  rust_libc_1.26.2_b64powerpc64.patch
21279b6412f3126cc7e8e14cf49cff42c035dfe979c1cb04baecc2ed71c1eb569537f5d1bf426169f146e8fcaa48dfece5ea2a047720cfeab82fbc6fbf770753  rust_libc_1.26.2_b64modrs.patch
7b622a6f7eb24e579211889998a5eed8c92fa011fdbc20ea60b7167b158c1d46fb33dc00ef3c4f2d2bd9aa265558d9af98f63c30a8b7185c358ccb3dfca487ee  rust_libc_1.26.2_b32modrs.patch
c31fdfe8a9b3411576c75da46645cf0465b9053000a2ab49cf9b2f2733f679d6d33acbf236d67a20e14935d094a685453b7f1840180249f39d610fd0902c3125  check-rustc"
