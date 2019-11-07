setup() {
	export ABUILD=../abuild
}

@test "help text" {
	$ABUILD -h
}

@test "version string" {
	$ABUILD -V
}
