#!/bin/sh

for i in "$@"; do
	case "$i" in
	 # encodings dir doesn't include fonts
		*/encodings) continue;
	esac
	mkfontdir "$i"
	mkfontscale "$i"
done

