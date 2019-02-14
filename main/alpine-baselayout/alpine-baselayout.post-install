#!/bin/sh

create_vserver_startstop() {
	cat <<__EOF__
#!/bin/sh

# This file is for compatibility
case \${0##*/} in
rcL)
	RUNLEVEL=1 /sbin/rc sysinit || exit 1
	/sbin/rc boot || exit 1
	/sbin/rc \${1:-default}
	exit 0
	;;
rcK)
	/sbin/rc shutdown
	;;
esac

__EOF__
}

# create compat start/stop scripts for vserver guests
if [ -x /sbin/rc ] && [ "$( /sbin/rc --sys )" = "VSERVER" ]; then
	# create rcL and rcK
	if ! [ -e /etc/init.d/rcL ]; then
		create_vserver_startstop > /etc/init.d/rcL
		chmod +x /etc/init.d/rcL
	fi
	if ! [ -e /etc/init.d/rcK ]; then
		ln -s rcL /etc/init.d/rcK
	fi
fi

# force /etc/shadow to be owned by root and not be world readable
chown root:shadow /etc/shadow
chmod 640 /etc/shadow

exit 0
