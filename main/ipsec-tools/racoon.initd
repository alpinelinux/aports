#!/sbin/runscript
# Copyright 1999-2004 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

depend() {
	before netmount
	use net
}

checkconfig() {
	if [ ! -e ${SETKEY_CONF} ] ; then
		eerror "You need to configure setkey before starting racoon."
		return 1
	fi
	if [ ! -e ${RACOON_CONF} ] ; then
		eerror "You need a configuration file to start racoon."
		return 1
	fi
	if [ ! -z ${RACOON_PSK_FILE} ] ; then
		if [ ! -f ${RACOON_PSK_FILE} ] ; then
			eerror "PSK file not found as specified."
			eerror "Set RACOON_PSK_FILE in /etc/conf.d/racoon."
			return 1
		fi
		case "`ls -Lldn ${RACOON_PSK_FILE}`" in
			-r--------*)
				;;
			*)
				eerror "Your defined PSK file should be mode 400 for security!"
				return 1
				;;
		esac
	fi
}

start() {
	checkconfig || return 1
	einfo "Loading ipsec policies from ${SETKEY_CONF}."
	/usr/sbin/setkey -f ${SETKEY_CONF}
	if [ $? -eq 1 ] ; then
		eerror "Error while loading ipsec policies"
	fi
	ebegin "Starting racoon"
	start-stop-daemon -S -x /usr/sbin/racoon -- -f ${RACOON_CONF} ${RACOON_OPTS}
	eend $?
}

stop() {
	ebegin "Stopping racoon"
	start-stop-daemon -K -p /var/run/racoon.pid
	eend $?
	if [ -n "${RACOON_RESET_TABLES}" ]; then
		ebegin "Flushing policy entries"
		/usr/sbin/setkey -F
		/usr/sbin/setkey -FP
		eend $?
	fi
}
