.TH THINKFAN.CONF 5 "November 2022" "thinkfan @THINKFAN_VERSION@"
.SH NAME
thinkfan.conf \- YAML-formatted config for
.BR thinkfan (1)



.SH DESCRIPTION

YAML is a very powerful, yet concise notation for structured data.
Its full specification is available at https://yaml.org/spec/1.2/spec.html.
Thinkfan uses only a small subset of the full YAML syntax, so it may be helpful,
but not strictly necessary for users to take a look at the spec.

The most important thing to note is that indentation is syntactically relevant.
In particular, tabs should not be mixed with spaces.
We recommend using two spaces for indentation, like it is shown below.

The thinkfan config has three main sections:

.TP
.B sensors:
Where temperatures should be read from. All
.BR hwmon -style
drivers are supported, as well as
.BR /proc/acpi/ibm/thermal ,
and, depending on the compile-time options,
.B libatasmart
(to read temperatures directly from hard disks) and
.B NVML
(via the proprietary nvidia driver).

.TP
.B fans:
Which fans should be used (currently only one allowed).
Support for multiple fans is currently in development and planned for a future
release.
Both
.BR hwmon -style
PWM controls and
.B /proc/acpi/ibm/fan
can be used.

.TP
.B levels:
Maps temperatures to fan speeds.
A \*(lqsimple mapping\*(rq just specifies one temperature as the lower and
upper bound (respectively) for a given fan speed.
In a \*(lqdetailed mapping\*(rq, the upper and lower bounds are specified for
each driver/sensor configured under
.BR sensors: .
This mode should be used when thinkfan is monitoring multiple devices that can
tolerate different amounts of heat.

.PP
Under each of these sections, there must be a list of key-value maps, each of
which configures a sensor driver, fan driver or fan speed mapping.


.SH SENSOR & FAN DRIVERS

For thinkfan to work, it first needs to know which temperature sensor drivers
and which fan drivers it should use.
The mapping between temperature readings and fan speeds is specified in a
separate config section (see the
.B FAN SPEEDS
section below).

.SS Sensor Syntax

The entries under the
.B sensors:
section can specify sysfs/hwmon, lm_sensors, thinkpad_acpi, NVML or atasmart drivers.
Support for lm_sensors, NVML and atasmart requires the appropriate libraries
and must have been enabled at compile time.
There can be any number (greater than zero) and combination of
.BR hwmon ,
.BR tpacpi ,
.BR nvml
and
.BR atasmart
entries.
However there may be at most one instance of the
.BR tpacpi
entry.

The syntax for identifying each type of sensors looks as follows:

.nf
\f[C]
########
\f[CB]sensors:
\f[CB]  \- hwmon: \f[CI]hwmon-path\f[CR]          # A path to a sysfs/hwmon sensor
\f[CB]    name: \f[CI]hwmon-name\f[CR]           # Optional entry
\f[CB]    model: \f[CI]hwmon-model\f[CR]         # Optional entry for nvme
\f[CB]    indices: \f[CI]index-list\f[CR]        # Optional entry

\f[CB]  \- chip: \f[CI]chip-name\f[CR]            # An lm_sensors/libsensors chip...
\f[CB]    ids: \f[CI]id-list\f[CR]               # ... with some feature IDs

\f[CB]  \- tpacpi: /proc/acpi/ibm/thermal\f[CR]  # Provided by the thinkpad_acpi kernel module
\f[CB]    indices: \f[CI]index-list\f[CR]        # Optional entry

\f[CB]  \- nvml: \f[CI]nvml-bus-id\f[CR]          # Uses the proprietary nVidia driver

\f[CB]  \- atasmart: \f[CI]disk-device-file\f[CR] # Requires libatasmart support

\f[CB]  \- \f[CR]...
\fR
.fi

Additionally, each sensor entry can have a number of settings that modify its
behavior:

.nf
\fC
########
\f[CB]sensors:
\f[CB]  \- \f[CR]...\f[CB]: \f[CR]... # A sensor specification as shown above
\f[CB]    correction: \f[CI]correction-list\f[CR]  # Optional entry
\f[CB]    optional: \f[CI]bool-ignore-errors\f[CR] # Optional entry
\f[CB]    max_errors: \f[CI]num-max-errors\f[CR]   # Optional entry
\fR
.fi


.SS Fan Syntax

Since version 2.0, thinkfan can control multiple fans.
So any number of
.B hwmon
fan sections can be specified.
Note however that the thinkpad_acpi kernel module only supports one fan, so
there can be at most one
.B tpacpi
section:

.nf
\fC
# ...
\f[CB]fans:
\f[CB]  \- tpacpi: /proc/acpi/ibm/fan

\f[CB]  \- hwmon: \f[CI]hwmon-path
\f[CB]    name: \f[CI]hwmon-name
\f[CB]    indices: \f[CI]index-list

\f[CB]  \- \f[CR]...
\fR
.fi

The behavior of any fan can optionally be controlled with the \fBoptional\fR
and \fBmax_errors\fR keywords, and by specifying a local fan speed config
under the \fBlevels:\fR keyword:

.nf
\fC
# ...
\f[CB]fans:
\f[CB]  \- \f[CR]...\f[CB]: \f[CR] ... # A fan specification as shown above
\f[CB]    optional: \f[CI]bool-ignore-errors\f[CR] # Optional entry
\f[CB]    max_errors: \f[CI]num-max-errors\f[CR]   # Optional entry
\f[CB]    levels: \f[CI]levels-section\f[CR]       # Optional entry


.SS Values

.TP
.I hwmon-path
There are three ways of specifying hwmon fans or sensors:

.RS
.IP 1) 3
A full path of a \*(lqtemp*_input\*(rq or \*(lqpwm*\*(rq file, like
\*(lq/sys/class/hwmon/hwmon0/pwm1\*(rq or
\*(lq/sys/class/hwmon/hwmon0/temp1_input\*(rq.
In this case, the \*(lq\c
.BI indices: " index-list"\c
\*(rq and \*(lq\c
.BI name: " hwmon-name"\c
\*(rq entries are unnecessary since the path uniquely identifies a specific fan or
sensor.

.RS
Note that this method may lead to problems when the load order of the drivers
changes across bootups, because in the \*(lqhwmon\fIX\fR\*(rq folder name, the
.I X
actually corresponds to the load order.
Use method 2) or 3) to avoid this problem.
.RE

.IP 2) 3
A directory that contains a specific hwmon driver, for example
\*(lq/sys/devices/platform/nct6775.2592\*(rq.
Note that this path does not contain the load-order dependent
\*(lqhwmon\fIX\fR\*(rq folder.
As long as it contains only a single hwmon driver/interface it is sufficient to
specify the
\*(lq\c
.BI indices: " index-list"\c
\*(rq
entry to tell thinkfan which specific sensors to use from that interface.
The
\*(lq\c
.BI name: " hwmon-name"\c
\*(rq
entry is unnecessary.

.P
.IP 3) 3
A directory that contains multiple or all of the hwmon drivers, for example
\*(lq/sys/class/hwmon\*(rq.
Here, both the \*(lq\c
.BI name: " hwmon-name"\c
\*(rq and \*(lq\c
.BI indices: " index-list"\c
\*(rq entries are required to tell thinkfan which interface to select below that
path, and which sensors or which fan to use from that interface.

.RE
.TP
.I hwmon-name
The name of a hwmon interface, typically found in a file called \*(lqname\*(rq.
This has to be specified if
.I hwmon-path
is a base path that contains multiple hwmons.
This method of specifying sensors is particularly useful if the full path to a
particular hwmon keeps changing between bootups, e.g. due to changing load order
of the driver modules.

.TP
.I hwmon-model
The model of a device in a hwmon interface usually found for NVME devices in a 
file under \*(lqdevice\*(rq called \*(lqmodel\*(rq.
For example, you might have an NVME \*(lq/sys/class/hwmon/hwmon3/device/model\*(rq
and you might have an external NVME over USB or Thunderbolt that you don't want
to monitor or you might have two NVME's.

.TP
.I index-list
A YAML list
.BI "[ "  X1  ", "  X2  ", " "\fR...\fB ]"
that specifies which sensors, resp. which fan to use from a given
interface.
Both
.B /proc/acpi/ibm/thermal
and also many hwmon interfaces contain multiple sensors, and not
all of them may be relevant for fan control.

.RS
.IP \(bu 2
For
.B hwmon
entries, this is required if
.I hwmon-path
does not refer directly to a single \*(lqtemp\fIXi\fR_input\*(rq file, but to a folder
that contains one or more of them.
In this case,
.I index-list
specifies the
.I Xi
for the \*(lqtemp\fIXi\fR_input\*(rq files that should be used.
A hwmon interface may also contain multiple PWM controls for fans, so in that case,
.I index-list
must contain exactly one entry.

.IP \(bu
For
.B tpacpi
sensors, this entry is optional.
If it is omitted, all temperatures found in
.B /proc/acpi/ibm/thermal
will be used.
.RE

.TP
.I chip-name
The unique name of an lm-sensors hwmon chip. lm-sensors or libsensors is the
official client library to access the sysfs hwmon subsystem, and as such it is
the preferred way of specifying sensors.
Available chips can be listed by running the \fBsensors\fR command as root.
The header above each block will be the chip name.

.TP
.I ids
A list of lm-sensors feature IDs, for example \*(lq\fB[ SYSTIN, CPUTIN, "Sensor 2"
]\fR\*(rq.
In the \fBsensors\fR command output, the name before the colon on each line
will be the feature ID.

.TP
.I nvml-bus-id
NOTE: only available if thinkfan was compiled with USE_NVML enabled.

The PCI bus ID of an nVidia graphics card that is run with the proprietary
nVidia driver. Can be obtained with e.g. \*(lqlspci | grep \-i vga\*(rq.
Usually, nVidia cards will use the open source
.B nouveau
driver, which should support hwmon sensors instead.

.TP
.I disk-device-file
NOTE: only available if thinkfan was compiled with USE_ATASMART enabled.

Full path to a device file for a hard disk that supports S.M.A.R.T.
See also the
.B \-d
option in
.BR thinkfan (1)
that prevents thinkfan from waking up sleeping (mechanical) disks to read their
temperature.

.TP
.IR correction-list " (optional, zeroes by default)"
A YAML list that specifies temperature offsets for each sensor in use by the
given driver. Use this if you want to use the \*(lqsimple\*(rq level syntax,
but need to compensate for devices with a lower heat tolerance.
Note however that the detailed level syntax is usually the better (i.e. more
fine-grained) choice.

.TP
.IR bool-ignore-errors " (optional, \fBfalse\fR by default)"
A truth value
.RB ( yes / no / true / false )
that specifies whether thinkfan should ignore all errors when using a given
sensor or fan.
Normally, thinkfan will exit with an error message if it fails reading
temperatures from a sensor or writing to a fan.

An optional device will not delay startup.
Instead, thinkfan will commence normal operation with the remaining devices and
re-try initializing unavailable optional devices in every loop until all are
found.

Marking a sensor/fan as optional may be useful for removable hardware or devices
that may get switched off entirely to save power.

.TP
.IR num-max-errors " (optional, \fB0\fR by default)"
A positive integer that specifies how often thinkfan is allowed to re-try if
if fails to initialize, read from or write to a given sensor or fan.
On startup, thinkfan will attempt to initialize the device \fInum-max-errors\fR
times before commencing normal operation.
If the device cannot be initialized after the given number of attempts,
thinkfan will fail.

When a device with a positive \fInum-max-errors\fR fails during runtime,
thinkfan will likewise attempt to re-initialize it the given number of times
before failing.

.TP
.IR levels-section " (optional, use global levels section by default)"
As of thinkfan 2.0, multiple fans can be configured.
To this end, each fan can now have its own \fBlevels:\fR section (cf. FAN
SPEEDS below).
The syntax of the global vs. the fan-specific \fBlevels:\fR section are
identical, the only difference being that in a fan-specific one, the speed
value(s) refer only the fans configured in that particular \fBfans:\fR entry.

NOTE: Global and fan-specific \fBlevels:\fR are mutually exclusive, i.e.
there cannot be both a global one and fan-specific sections.


.SH FAN SPEEDS

The
.B levels:
section specifies a list of fan speeds with associated lower and upper
temperature bounds.
If temperature(s) drop below the lower bound, thinkfan switches to the previous
level, and if the upper bound is reached, thinkfan switches to the next level.

Since thinkfan 2.0, this section can appear either under an individual fan
(cf. \fBlevels:\fR keyword under \fBFan Syntax\fR), or globally.

.SS Simple Syntax
In the simplified form, only one temperature is specified as an upper/lower
limit for a given fan speed.
In that case, the
.I lower-bound
and
.I upper-bound
are compared only to the highest temperature found among all configured sensors.
All other temperatures are ignored.
This mode is suitable for small systems (like laptops) where there is only one
device (e.g. the CPU) whose temperature needs to be controlled, or where the
required fan behaviour is similar enough for all heat-generating devices.

.nf
\fC
# ...
\f[CB]levels:
\f[CB]  \- [ \f[CI]fan-speed\f[CB], \f[CI]lower-bound\f[CB], \f[CI]upper-bound\f[CB] ]
\f[CB]  \- \f[CR]...
\fR
.fi


.SS Detailed Syntax
This mode is suitable for more complex systems, with devices that have
different temperature ratings.
For example, many modern CPUs and GPUs can deal with temperatures above
80\[char176]C on a daily basis, whereas a hard disk will die quickly if it
reaches such temperatures.
In detailed mode, upper and lower temperature limits are specified for each
sensor individually:

.nf
\fC
# ...
\f[CB]levels:
\f[CB]  \- speed: [ \f[CI]fan1-speed\f[CB], \f[CI]fan2-speed\f[CB], \f[CR]...\f[CB] ]
\f[CB]    lower_limit: [ \f[CI]l1\f[CB], \f[CI]l2\f[CB], \f[CR]...\f[CB] ]
\f[CB]    upper_limit: [ \f[CI]u1\f[CB], \f[CI]u2\f[CB], \f[CR]...\f[CB] ]
\f[CB]  \- \f[CR]...\f[CB]
\fR
.fi


.SS Values

.TP
.IB fan1-speed ", " fan2-speed ", " \fR...
.TP
.I fan-speed
When multiple fans are specified under the
.B fans:
section, value of the
.B speed:
keyword must be a list of as many
.I fanX-speed
values.
They are applied to the fans by their order of appearance, i.e. the first
speed value applies to the fan that has been specified first, the second value
to the second fan, and so on.
If there is just one fan, instead of a list with just one element, the speed
value can be given as a scalar.

The possible speed values for
.B fanX-speed
are different depending on which fan driver is used:

.RS
.IP \(bu 3
For a
.B hwmon
fan,
.I fanX-speed
is a numeric value ranging from
.B 0
to
.BR 255 ,
corresponding to the PWM values accepted by the various kernel drivers.

.IP \(bu
For a
.B tpacpi
fan on Lenovo/IBM ThinkPads and some other Lenovo laptops (see \fBSENSORS & FAN
DRIVERS\fR above), numeric values and strings can be used.
The numeric values range from 0 to 7.
The string values take the form \fB"level \fIlvl-id\fB"\fR, where
.I lvl-id
may be a value from
.BR 0 " to " 7 ,
.BR auto ,
.B full-speed
or
.BR disengaged .
The numeric values
.BR 0 " to " 7
correspond to the regular fan speeds used by the firmware, although many
firmwares don't even use level \fB7\fR.
The value \fB"level auto"\fR gives control back to the firmware, which may be
useful if the fan behavior only needs to be changed for certain specific
temperature ranges (usually at the high and low end of the range).
The values \fB"level full-speed"\fR and \fB"level disengaged"\fR take the fan
speed control away from the firmware, causing the fan to slowly ramp up to an
absolute maximum that can be achieved within electrical limits.
Note that this will run the fan out of specification and cause increased wear,
though it may be helpful to combat thermal throttling.
.RE

.TP
.IB l1 ", " l2 ", " \fR...
.TP
.IB u1 ", " u2 ", " \fR...
The lower and upper temperature limits refer to the sensors in the same order
in which they were found when processing the
.B sensors:
section (see
.B SENSOR & FAN DRIVERS
above).
For the first level entry, the
.B lower_limit
may be omitted, and for the last one, the
.B upper_limit
may be omitted.
For all levels in between, the lower limits must overlap with the upper limits
of the previous level, to make sure the entire temperature range is covered and
that there is some hysteresis between speed levels.

Instead of a temperature, an underscore (\fB_\fR) can be given.
An underscore means that the temperature of that sensor should be ignored at
the given speed level.


.SH SEE ALSO
The thinkfan manpage:
.BR thinkfan (1)

.nf
Example configs shipped with the source distribution, also available at:
.UR https://github.com/vmatare/thinkfan/tree/master/examples
.UE
.fi

.nf
The Linux hwmon user interface documentation:
.UR https://www.kernel.org/doc/html/latest/hwmon/sysfs\-interface.html
.UE
.fi

.nf
The thinkpad_acpi interface documentation:
.UR https://www.kernel.org/doc/html/latest/admin\-guide/laptops/thinkpad\-acpi.html
.UE
.fi

.SH BUGS

.nf
Report bugs on the github issue tracker:
.UR https://github.com/vmatare/thinkfan/issues
.UE
.fi

