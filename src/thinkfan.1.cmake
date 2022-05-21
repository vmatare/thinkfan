.TH THINKFAN "5" "April 2022" "thinkfan @THINKFAN_VERSION@" "thinkfan"
.SH NAME
thinkfan \- A simple fan control program
.SH SYNOPSIS
.SY thinkfan
.OP \-hnqDd
.OP \-b BIAS
.OP \-c CONFIG
.OP \-s SECONDS
.OP \-p \fR[\fIDELAY\fR]\fI
.YS



.SH DESCRIPTION

Thinkfan reads temperatures from a configured set of sensors and then sets fan
speeds according to temperature limits set in the config file.

.HP
\fBWARNING\fR: Thinkfan does only very basic sanity checking on the
configuration. This means that a bad configuration can increase thermal wear
on the hardware or even cause destruction from overheating!


.SS Supported sensors

.TP
\(bu /proc/acpi/ibm/thermal
This is provided by the thinkpad_acpi kernel module on older Thinkpads.

.TP
\(bu temp*_input files in sysfs (hwmon interface)
May be provided by any hwmon drivers, including thinkpad_acpi on modern
Thinkpads.

.TP
\(bu Hard disks with S.M.A.R.T. support (libatasmart)
Available if thinkfan was compiled with
.B \-DUSE_ATASMART=ON.
Note that modern Linux kernels can also expose S.M.A.R.T. hard disk
temperatures via the hwmon interface in sysfs (and therefore also via
lm_sensors), which is generally preferrable because it is more efficient.

.TP
\(bu From the proprietary nVidia driver
When the proprietary nVidia driver is used, no hwmon for the card will be
available. In this situation, thinkfan can use the proprietary NVML API to get
temperatures.

.TP
\(bu Via lm_sensors (libsensors interface)
This is a modern and more reliable alternative to the sysfs hwmon interface
mentioned above. It's basically a standardized abstraction for sysfs hwmon
where sensors can always be identified uniquely, even when the load order of
kernel modules changes.

.SS Supported fans
Thinkfan can control any number of fans, which can be specified in two ways:

.TP
\(bu /proc/acpi/ibm/fan
Provided by the thinkpad_acpi kernel module. Note that the kernel module needs
to be loaded with the option "fan_control=1" to enable userspace fan control.
On some models, "experimental=1" may also be required. See the
.B SEE ALSO
section at the bottom of this manpage for a link to the official thinkpad_acpi
documentation.

.TP
\(bu pwm*_enable and pwm? files in sysfs
Provided by all modern hardware monitoring drivers, including thinkpad_acpi.


.SS Mapping temperatures to fan speeds

There are two general modes of operation:

.TP
\(bu Detailed mode
In detailed mode, temperature limits are defined for each sensor thinkfan knows
about. Setting suitable limits for each sensor in your system will probably
require a bit of experimentation and good knowledge about your hardware, but
it's the safest way of keeping each component within its specified temperature
range. See the example configs to learn about the syntax.

.TP
\(bu Simple mode
In simple mode, thinkfan uses only the highest temperature found in the
system. That may be dangerous, e.g. for hard disks.
That's why you should provide a correction value (i.e. add 10\-15 \[char176]C)
for the sensor that has the temperature of your hard disk (or battery...).
See the example config files for details about that.




.SH CONFIGURATION
All of the features described above are configured in the thinkfan config
file. Its default location is
.B /etc/thinkfan.conf
or
.BR /etc/thinkfan.yaml
(see also the
.B -c
option below).
An example configuration is provided with the source package. It is intended
purely for illustration of various scenarios and is not suitable as a basis
for actually functional config. For a complete reference see the config man
page
.BR thinkfan.conf (5).



.SH OPTIONS

.TP
.B \-h
Show a short help message

.TP
.BI \-s " SECONDS"
Maximum seconds between temperature updates (default: 5)

.TP
.BI \-b " BIAS"
Floating point number (\-10 to 30) to smooth out or amplify quick temperature
changes.
If a sensor's temperature increases by more than 2 \[char176]C during one
cycle, we calculate an offset value as follows:

    \fBoffset\fR = \fBdelta_t\fR * \fIBIAS\fR / 10

This offset is then added to the actual temperature:

    \fBbiased_t\fR = \fBcurrent_t\fR + \fBoffset\fR

If \fBdelta_t\fR stays below 2 \[char176]C in subsequent loops, \fBoffset\fR
will be reduced back to 0 in increments of sgn(\fIBIAS\fR) * (1 +
abs(\fIBIAS\fR/5)).

This means that a negative \fIBIAS\fR will even out short and sudden
temperature spikes like those seen on some on\-DIE sensors, while positive
values will exaggerate increasing temperatures to compensate e.g. for sensors
that respond slowly because they are attached to heavy heatsinks.

Use DANGEROUS mode
to remove the \-10 to +30 limit. Note that you can't have a space between \-b
and a negative argument, because otherwise getopt will interpret things like
\-10 as an option and fail (i.e. write
.B \-b\-10
instead of
.BR "\-b \-10" ).

The default is 0.

.TP
.BI \-c " FILE"
Load a different configuration file.
By default, thinkfan first tries to load /etc/thinkfan.yaml, and
/etc/thinkfan.conf after that.
The former must be in YAML format, while the latter can be either YAML or the
old legacy syntax.

If this option is specified, thinkfan attempts to load the config only from
.IR FILE .
If its name ends in \*(lq.yaml\*(rq, it must be in YAML format.
Otherwise, it can be either YAML or legacy syntax.
See
.BR thinkfan.conf (5)
and
.BR thinkfan.conf.legacy (5)
for details.

.TP
.B \-n
Do not become a daemon and log to terminal instead of syslog

.TP
.B \-q
Be quiet, i.e. reduce logging level from the default. Can be specified
multiple times until only errors are displayed/logged.

.TP
.B \-v
Be more verbose. Can be specified multiple times until every message is
displayed/logged.

.TP
.BR "\-p " [\fISECONDS\fR]
Use the pulsing\-fan workaround (for older Thinkpads). Takes an optional
floating\-point argument (0\-10s) as depulsing duration. Default 0.5s.

.TP
.B \-d
Do not read temperature from sleeping disks. Instead, 0 \[char176]C is used as that
disk's temperature. This is needed if reading the temperature causes your
disk to wake up unnecessarily.
NOTE: This option is only available if thinkfan was built with \-D USE_ATASMART.

.TP
.B \-D
DANGEROUS mode: Disable all sanity checks. May damage your hardware!!



.SH SIGNALS
SIGINT and SIGTERM simply interrupt operation and should cause thinkfan to
terminate cleanly.
.P
SIGHUP makes thinkfan reload its config. If there's any problem with the new
config, we keep the old one.
.P
SIGUSR1 causes thinkfan to dump all currently known temperatures either to
syslog, or to the console (if running with the \-n option).
.P
SIGPWR tells thinkfan that the system is about to go to sleep. Thinkfan will
then allow sensor read errors for the next 4 loops because many sensors will
take a few seconds before they are available again after waking up from a
sleep state (suspend or hibernate). If the shipped systemd service file
.B thinkfan-sleep.service
is installed, it should take care of sending this singal when going to sleep.
On non-systemd distributions, other mechanisms may have to be used.
.P
SIGUSR2 tells thinkfan to re-initialize fan control. This is required by most
fan drivers after waking up from suspend because they tend to reset fan
control to automatic mode on wakeup. Similar to SIGPWR, the systemd service
file
.B thinkfan-wakeup.service
should take care of sending this signal on wakeup on systemd systems. On
non-systemd distributions, other mechanisms may have to be used.


.SH RETURN VALUE

.TP
.B 0
Normal exit

.TP
.B 1
Runtime error

.TP
.B 2
Unexpected runtime error

.TP
.B 3
Invalid commandline option



.SH SEE ALSO
.nf
The thinkfan config manpage:
.BR thinkfan.conf (5)

Example configs shipped with the source distribution, also available at:
.hy 0
https://github.com/vmatare/thinkfan/tree/master/examples

The Linux hwmon user interface documentation:
https://www.kernel.org/doc/html/latest/hwmon/sysfs\-interface.html

The thinkpad_acpi interface documentation:
https://www.kernel.org/doc/html/latest/admin\-guide/laptops/thinkpad\-acpi.html



.SH BUGS
If thinkfan tells you to, or if you feel like it, report issues at the Github
issue tracker:

.hy 0
https://github.com/vmatare/thinkfan/issues

