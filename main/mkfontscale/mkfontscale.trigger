#!/bin/sh

for i in "$@"; do
	case "$i" in
	 # encodings dir doesn't include fonts
		*/encodings) continue;
	esac
	[ -d "$i" ] || continue
	rm -f "$i"/fonts.dir "$i"/fonts.scale
	rmdir "$i" 2>/dev/null || {
		mkfontdir "$i"
		mkfontscale "$i"
	}
done
