#!/bin/sh

fail=0

help_output=$(../src/apk version --help 2>/dev/null)
invalid_option_output="$(../src/apk --invalid-option version 2>/dev/null)"
if [ "$help_output" != "$invalid_option_output" ]; then
	echo "FAIL: invalid option does not trigger help"
	fail=$((fail+1))
fi

if ! ../src/apk --help 2>/dev/null | grep -q "^Use apk --help --verbose for a full command listing."; then
	echo "FAIL: brief help gives long help"
	fail=$((fail+1))
fi

if ../src/apk --help --verbose 2>/dev/null | grep -q "^Use apk --help --verbose for a full command listing."; then
	echo "FAIL: long help does not work"
	fail=$((fail+1))
fi

if [ $fail -eq 0 ]; then
	echo "OK: command parsing works"
fi

exit $fail
