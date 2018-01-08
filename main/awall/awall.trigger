#!/bin/sh

# trigger script for awall
# Copyright (c) 2014-2017 Kaarle Ritvanen

[ -f /etc/iptables/awall-save ] || exit 0

exec /usr/sbin/awall translate
