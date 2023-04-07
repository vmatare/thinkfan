[Unit]
Description=thinkfan @THINKFAN_VERSION@
Documentation=man:thinkfan
After=sysinit.target
After=systemd-modules-load.service

[Service]
Type=forking
ExecStart=@CMAKE_INSTALL_PREFIX@/sbin/thinkfan $THINKFAN_ARGS
PIDFile=/run/thinkfan.pid
ExecReload=/bin/kill -HUP $MAINPID

[Install]
WantedBy=multi-user.target
Also=thinkfan-sleep.service
Also=thinkfan-wakeup.service
