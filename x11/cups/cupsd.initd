#!/sbin/runscript

depend() {
	use net
	@neededservices@
	before nfs
	after logger
}

start() {
	ebegin "Starting cupsd"
	start-stop-daemon --start --quiet --exec /usr/sbin/cupsd
	eend $?
}

stop() {
	ebegin "Stopping cupsd"
	start-stop-daemon --stop --quiet --exec /usr/sbin/cupsd
	eend $?
}
