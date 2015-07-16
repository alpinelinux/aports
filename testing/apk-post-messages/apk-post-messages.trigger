#!/bin/sh

x=README.alpine
for i; do
	if [ -f "$i/$x" ]; then
		msg="| $i: $x |"
		msg_len=$(( $(echo $msg |wc -m) - 1))
		printf "%${msg_len}s" | sed 's/ /-/g'
		echo -e "\n$msg"
		printf "%${msg_len}s" | sed 's/ /=/g'
		echo -e "\n"
		cat $i/$x
		echo -e; break
	fi
done

