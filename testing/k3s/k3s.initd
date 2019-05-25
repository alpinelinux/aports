#!/sbin/openrc-run

name="k3s"
command="/usr/bin/k3s"
command_args="$K3S_OPTS"
command_background="yes"

start_stop_daemon_args="server"
pidfile="/run/k3s.pid"

depend() {
	need net
	after firewall
}
