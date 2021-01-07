setup() {
	export ABUILD="$PWD/../abuild"
	export ABUILD_SHAREDIR="$PWD/.."
	export ABUILD_CONF=/dev/null
	export REPODEST="$BATS_TMPDIR"/packages
}

teardown() {
	rm -rf "$REPODEST"
}

@test "abuild: help text" {
	$ABUILD -h
}

@test "abuild: version string" {
	$ABUILD -V
}

@test "abuild: build simple package without deps" {
	cd testrepo/pkg1
	$ABUILD
}

@test "abuild: build failure" {
	cd testrepo/buildfail
	run $ABUILD cleanpkg all
	[ $status -ne 0 ]
}

@test "abuild: test check for /usr/lib64" {
	cd testrepo/lib64test
	run $ABUILD cleanpkg all
	[ $status -ne 0 ]
}

@test "abuild: test options=lib64" {
	cd testrepo/lib64test
	options=lib64 $ABUILD
	$ABUILD cleanpkg
}

@test "abuild: verify that packages are reproducible built" {
	cd testrepo/pkg1
	arch=$($ABUILD -A)
	pkgs=$($ABUILD listpkg)

	$ABUILD
	checksums=$(cd "$REPODEST"/testrepo/$arch && md5sum $pkgs)
	echo "$checksums"

	$ABUILD cleanpkg all
	checksums2=$(cd "$REPODEST"/testrepo/$arch && md5sum $pkgs)
	echo "$checksums2"

	[ "$checksums" = "$checksums2" ]
}
