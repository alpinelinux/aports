# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

[ -e /etc/conf.d/udev ] && . /etc/conf.d/udev

compat_volume_nodes()
{
	# Only do this for baselayout-1*
	# This check is likely to get false positives due to some multilib stuff,
	# but that should not matter, as this can only happen on old openrc versions
	# no longer available as ebuilds.
	if [ ! -e /lib/librc.so ]; then

		# Create nodes that udev can't
		[ -x /sbin/lvm ] && \
			/sbin/lvm vgscan -P --mknodes --ignorelockingfailure &>/dev/null
		# Running evms_activate on a LiveCD causes lots of headaches
		[ -z "${CDBOOT}" -a -x /sbin/evms_activate ] && \
			/sbin/evms_activate -q &>/dev/null
	fi
}

start_initd()
{
	(
		. /etc/init.d/"$1"
		_start
	)
}

# mount tmpfs on /dev
start_initd udev-mount || exit 1

# Create a file so that our rc system knows it's still in sysinit.
# Existance means init scripts will not directly run.
# rc will remove the file when done with sysinit.
# this is no longer needed as of openrc-0.4.0
touch /dev/.rcsysinit

# load device tarball
start_initd udev-dev-tarball

# run udevd
start_initd udev || exit 1

compat_volume_nodes

# inject into boot runlevel
IN_HOTPLUG=1 /etc/init.d/udev-postmount start >/dev/null 2>&1

# udev started successfully
