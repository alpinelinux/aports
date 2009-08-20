#!/bin/sh

for i in "$@"; do
	# get last element in path
	kver=${i##*/}
	mkinitfs $kver
done

