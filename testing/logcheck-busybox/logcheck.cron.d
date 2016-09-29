#!/bin/sh

# fix permissions
# busybox syslog => add logcheck to group wheel
# ---------------------------------------------------------------
if [ -d /var/log/socklog ]; then
        find /var/log/socklog -type d -exec chown :logcheck {} \;
        chown :logcheck /var/log/socklog/*/current
fi

chown -R logcheck /etc/logcheck
# ---------------------------------------------------------------

if [ "$1" = "reboot" ]; then
        REBOOT="-R"
fi

if [ -x /usr/sbin/logcheck ]; then
        if [ ! -d /etc/pam.d ]; then
                # libpam does not work with a subshell
                su -s /bin/sh -c "nice -n10 /usr/sbin/logcheck ${REBOOT} &" logcheck
        else
                # sudo works without modification
                if [ -x /usr/bin/sudo ]; then
                        sudo -u logcheck nice -n10 /usr/sbin/logcheck ${REBOOT} &
                else
                        logger "install sudo to use logcheck with linux-pam"
                fi
        fi
fi

