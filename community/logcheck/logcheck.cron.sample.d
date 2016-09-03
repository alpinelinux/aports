## non busybox cron:
## /etc/cron.d/logcheck: crontab entries for the logcheck package

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
MAILTO=root

@reboot         logcheck    if [ -x /usr/sbin/logcheck ]; then nice -n10 /usr/sbin/logcheck -R; fi
2 * * * *       logcheck    if [ -x /usr/sbin/logcheck ]; then nice -n10 /usr/sbin/logcheck; fi

# EOF


## busybox crond reboot script [ /etc/local.d/logcheck.start ]

#!/bin/sh

# fix permissions
# busybox syslog => add logcheck to group wheel
#----------------------------------------------
if [ -x /usr/sbin/logcheck ]; then
        if [ ! -d /etc/pam.d ]; then
                # libpam does not work with a subshell
                su -s /bin/sh -c "nice -n10 /usr/sbin/logcheck -R" logcheck
        else
                # sudo works without modification
                if [ -x /usr/bin/sudo ]; then
                        sudo -u logcheck nice -n10 /usr/sbin/logcheck -R
                else
                        logger "*** ERROR: logcheck with linux-pam requires sudo ***"
                fi
        fi
fi

# EOF

## sSMTP configuration

addgroup logcheck mail
chown :mail /etc/ssmtp/ssmtp.conf
chmod 640 /etc/ssmtp/ssmtp.conf

## socklog configuration [ /etc/sv/socklog-unix/log/run ]

#!/bin/sh
exec chpst -ulog:logcheck svlogd \
  main/main main/auth main/cron main/daemon main/debug main/ftp \
  main/kern main/local main/mail main/news main/syslog main/user

#EOF
