#!/sbin/openrc-run

extra_started_commands="reload"

depend() {
	after modules
}

start() {
	ebegin "Starting thinkfan"
	start-stop-daemon --start --exec @CMAKE_INSTALL_PREFIX@/sbin/thinkfan -- -q -s5 -c /etc/thinkfan.conf
	eend $?
}

stop() {
	ebegin "Stopping thinkfan"
	start-stop-daemon --stop --exec @CMAKE_INSTALL_PREFIX@/sbin/thinkfan
	eend $?
}

reload() {
	PID=$(<@PID_FILE@)
	ebegin "Sending SIGHUP to thinkfan($PID)"
	kill -HUP $PID
	eend $?
}
