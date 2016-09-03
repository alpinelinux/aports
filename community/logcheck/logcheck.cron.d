#!/bin/sh

# fix permissions
# busybox syslog => add logcheck to group wheel
# ---------------------------------------------
if [ -x /usr/sbin/logcheck ]; then
        if [ ! -d /etc/pam.d ]; then
                # libpam does not work with a subshell
                su -s /bin/sh -c "nice -n10 /usr/sbin/logcheck" logcheck
        else
                # sudo works without modification
                if [ -x /usr/bin/sudo ]; then
                        sudo -u logcheck nice -n10 /usr/sbin/logcheck
                else
                        echo "*** ERROR: logcheck with linux-pam requires sudo ***"
                fi
        fi
fi
