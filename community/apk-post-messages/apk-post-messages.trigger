#!/bin/sh

x=README.alpine
for i; do
	if [ -f "$i/$x" ]; then
		msg="| $i: $x |"
		msg_len=$(printf '%s' "$msg" | wc -m)
		printf "%${msg_len}s" | sed 's/ /-/g'
		printf '\n%s\n' "$msg"
		printf "%${msg_len}s" | sed 's/ /=/g'
		printf '\n\n'
		cat -- "$i/$x"
		printf '\n'
	fi
done
