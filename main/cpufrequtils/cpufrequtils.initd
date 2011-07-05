#!/sbin/runscript
# Copyright 1999-2008 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-power/cpufrequtils/files/cpufrequtils-init.d-005,v 1.2 2008/10/21 21:20:59 vapier Exp $

affect_change() {
	local c ret=0
	ebegin "Running cpufreq-set $*"
	for c in $(cpufreq-info -o | awk '$1 == "CPU" { print $2 }') ; do
		cpufreq-set -c ${c} $*
		: $((ret+=$?))
	done
	eend ${ret}
}

start() {
	affect_change ${START_OPTS}
}

stop() {
	affect_change ${STOP_OPTS}
}
