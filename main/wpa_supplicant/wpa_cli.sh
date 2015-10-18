#!/bin/sh
# Distributed under the terms of the BSD License.
# Copyright (c) 2015 Sören Tempel <soeren+alpine@soeren-tempel.net>

IFUP="/sbin/ifup"
IFDOWN="/sbin/ifdown"

if [ -z "${1}" -o -z "${2}" ]; then
	logger -t wpa_cli "this script should be called from wpa_cli(8)"
	exit 1
elif ! [ -x "${IFUP}" -a -x "${IFDOWN}" ]; then
	logger -t wpa_cli "${IFUP} or ${IFDOWN} doesn't exist"
	exit 1
fi

IFNAME="${1}"
ACTION="${2}"

EXEC=""
case "${ACTION}" in
	CONNECTED)
		EXEC="${IFUP}"
		;;
	DISCONNECTED)
		EXEC="${IFDOWN}"
		;;
	*)
		logger -t wpa_cli "unknown action '${ACTION}'"
		exit 1
esac

logger -t wpa_cli "interface ${IFNAME} ${ACTION}"
${EXEC} "${IFNAME}" || logger -t wpa_cli "executing '${EXEC}' failed"
