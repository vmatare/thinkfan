[Unit]
Description=Thinkfan, the minimalist fan control program
After=sysinit.target

[Service]
Type=forking
ExecStart=@CMAKE_INSTALL_PREFIX@/sbin/thinkfan $THINKFAN_ARGS
PIDFile=/run/thinkfan.pid
ExecReload=/bin/kill -HUP $MAINPID

[Install]
WantedBy=multi-user.target
Also=thinkfan-wakeup.service
