
init_tests() {
	TESTS="$@"
	export TESTS
	for t; do
		atf_test_case $t
	done
}

atf_init_test_cases() {
	for t in $TESTS; do
		atf_add_test_case $t
	done
}

