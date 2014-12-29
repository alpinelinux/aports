#!/bin/sh

rm -f /usr/share/man/mandoc.db
exec /usr/sbin/makewhatis -a /usr/share/man
