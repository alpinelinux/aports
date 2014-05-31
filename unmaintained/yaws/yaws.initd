#!/sbin/runscript
#
# Startup script for the Yaws Web Server (for Alpine Linux)
#
# config: /etc/conf.d/yaws
#
# description: yaws - Erlang enabled http server
#
# use: rc-update add yaws default
#

yaws=/usr/bin/yaws

# By default we run with the default id
# idopts="--id myserverid"

conf="--conf /etc/yaws/yaws.conf"

extra_started_commands="restart reload"
extra_commands="query"

depend() {
       need net
}


start() {
       ebegin "Starting yaws "
       ${yaws} --daemon --heart ${conf}
       eend $?
}


stop() {
       ebegin "Stopping yaws "
       ${yaws} --stop ${idopts}
       eend $? "Failed to stop yaws"
}


reload() {
       ebegin "Reloading yaws "
       ${yaws} --hup ${idopts}
       eend $? "Failed to reload yaws"
}

query() {
       ebegin "Querying yaws "
       ${yaws} --status ${idopts}
       eend $?
}

