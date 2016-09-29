## non busybox cron:
## /etc/cron.d/logcheck: crontab entries for the logcheck package

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
MAILTO=root

@reboot         logcheck    if [ -x /usr/sbin/logcheck ]; then nice -n10 /usr/sbin/logcheck -R; fi
2 * * * *       logcheck    if [ -x /usr/sbin/logcheck ]; then nice -n10 /usr/sbin/logcheck; fi

# EOF

## busybox cron:

see included:

/etc/init.d/logcheck (for @reboot functionality)
/etc/periodic/hourly/logcheck


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
