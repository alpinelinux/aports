#!/bin/sh

msg() {
	local summary="$1"
	shift
	echo "" >&2
	echo "  $summary" >&2
	local i
	for i; do
		echo "     $i" >&2
	done
	echo "" >&2
}

# compare the timestamp of "started" symlink with timestamp of /etc/init.d
# if the /etc/init.d/* script is newer than "started" symlink then
# service was upgraded after service was started
services=$(find /run/openrc/started  -type l | xargs stat -c "%n %Y" \
		| while read file started; do
	svc=${file##*/}
	installed=$(stat -c "%Y" /etc/init.d/$svc)
	if [ $installed -gt $started ]; then
		echo $svc
	fi
done)

need_reboot=false
for i; do
	case $i in
	/boot)
		need_reboot=true
	esac
done

notify=msg
if which notify-send > /dev/null; then
	notify="notify-send"
fi

if [ -n "$services" ]; then
	$notify "The following services have been updated and need a restart:" \
		$services
fi

case "$(rc --sys)" in
	LXC|VSERVER) exit 0 ;;
esac

if $need_reboot ; then
	$notify "Kernel(s) were updated. You might need to reboot"
fi
	
