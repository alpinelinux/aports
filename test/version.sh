#!/bin/sh

fail=0
while read a result b rest ; do
	output="$(../src/apk version -t "$a" "$b")"
	if [ "$output" != "$result" ] ; then
		echo "$a $result $b, but got $output"
		fail=$(($fail+1))
	fi
done

exit $fail

