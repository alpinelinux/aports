#!/bin/sh

for i in "$@"; do
	mkfontdir "$i"
	mkfontscale "$i"
done

