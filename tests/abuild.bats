setup() {
	export ABUILD=../abuild
	export ABUILD_SHAREDIR=$PWD/..
}

@test "abuild: help text" {
	$ABUILD -h
}

@test "abuild: version string" {
	$ABUILD -V
}
