setup() {
	export ABUILD=../abuild
	export ABUILD_SHAREDIR=$PWD/..
}

@test "help text" {
	$ABUILD -h
}

@test "version string" {
	$ABUILD -V
}
