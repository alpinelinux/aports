setup() {
	export FUNCS=../functions.sh
}

@test "check if CBUILD is set" {
	. $FUNCS && test -n "$CBUILD"
}

@test "check that missing gcc does not kill us" {
	sh -e -c "CC=false; . $FUNCS && test -z \"$CBUILD\""
}
