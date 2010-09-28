#!/bin/sh

for i in "$@"; do
	gtk-update-icon-cache -q -t -f $i
done

