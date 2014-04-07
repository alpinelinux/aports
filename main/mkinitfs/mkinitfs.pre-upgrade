#!/bin/sh

mkdir -p /etc/mkinitfs/features.d
for i in files modules; do
	for j in /etc/mkinitfs/$i.d/*; do
		[ -e "$j" ] || continue
		case "$j" in
		*.apk-new) continue;;
		esac
		mv $j /etc/mkinitfs/features.d/${j##*/}.$i
	done
done
exit 0
