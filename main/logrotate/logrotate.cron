#!/bin/sh

if [ -f /etc/conf.d/logrotate ]; then
	. /etc/conf.d/logrotate
fi

if [ -x /usr/bin/cpulimit ] && [ -n "$CPULIMIT" ]; then
	_cpulimit="/usr/bin/cpulimit --limit=$CPULIMIT"
fi

$_cpulimit /usr/sbin/logrotate /etc/logrotate.conf
EXITVALUE=$?
if [ $EXITVALUE != 0 ]; then
    /usr/bin/logger -t logrotate "ALERT exited abnormally with [$EXITVALUE]"
fi
exit 0
