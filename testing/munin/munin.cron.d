# /etc/cron.d/munin: crontab entries for the munin package

PATH=/sbin:/bin:/usr/sbin:/usr/bin
MAILTO=root

@reboot         munin if [ -x /usr/bin/munin-cron ]; then /usr/bin/munin-cron; fi
*/5 * * * *     munin if [ -x /usr/bin/munin-cron ]; then /usr/bin/munin-cron; fi

# EOF
