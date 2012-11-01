#!/bin/sh

# in 0.8.0-r1 the state dir moved from /libexec/rc/init.d to /lib/rc/init.d
# and with 0.10 it moved to /run/openrc

mkdir -p /run/openrc
for dir in /libexec /lib; do
	[ -d $dir/rc/init.d ] || continue

	for i in $dir/rc/init.d/* ; do
		[ -e "$i" ] || continue
		if [ -e /run/openrc/${i##*/} ]; then
			rm -r $i
		else
			mv $i /run/openrc/
		fi
	done

	rmdir $dir/rc/init.d $dir/rc /libexec 2>/dev/null
done

# create rc.local compat
if [ -f /etc/rc.local ]; then
	cat >/etc/local.d/rc.local-compat.start<<__EOF__
#!/bin/sh

# this is only here for compatibility reasons
if [ -f /etc/rc.local ]; then
	. /etc/rc.local
fi
__EOF__
	chmod +x /etc/local.d/rc.local-compat.start
fi

exit 0
