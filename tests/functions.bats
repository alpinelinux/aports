setup() {
	export FUNCS=../functions.sh
}

@test "functions: check if CBUILD is set" {
	. $FUNCS && test -n "$CBUILD"
}

@test "functions: check that missing gcc does not kill us" {
	sh -e -c "CC=false; . $FUNCS && test -z \"$CBUILD\""
}
