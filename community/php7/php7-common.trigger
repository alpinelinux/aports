#!/bin/sh

find -L /usr/bin -type l -name "*php*" -delete

for file in $(find /usr/bin -type f -name "*php7*"); do
	[ ! -e ${file/7/} ] && ln -sf $file ${file/7/}
done

exit 0
