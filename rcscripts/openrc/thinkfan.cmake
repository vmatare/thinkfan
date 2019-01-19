#!/sbin/openrc-run

command="@CMAKE_INSTALL_PREFIX@/sbin/thinkfan"
command_args="-q -s5 -c /etc/thinkfan.conf"
pidfile="@PID_FILE@"

extra_started_commands="reload"

required_files="/etc/thinkfan.conf"

depend() {
	after modules
}

reload() {
	ebegin "Reloading ${SVCNAME}"
	start-stop-daemon --signal HUP --pidfile "${pidfile}"
	eend $?
}
