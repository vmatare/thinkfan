.TH THINKFAN.CONF.LEGACY 5 2020-04-09 "thinkfan @THINKFAN_VERSION@"
.SH NAME
thinkfan.conf.legacy \- the old, backwards-compatible config syntax for
thinkfan
.BR thinkfan (1)

.SH DESCRIPTION
The thinkfan config file specifies one or more temperature input(s), exactly
one fan to control and the fan levels.
A fan level associates a certain fan speed to a lower and an upper
temperature bound.
If the temperature reaches the upper bound, we switch to the next fan level,
and if it drops below the lower bound, we switch to the previous fan level.
Temperature bounds can either be a single temperature (\fIsimple mode\fR) or
consist of multiple temperatures (\fIcomplex mode\fR).
In simple mode, only the highest of all known temperature is compared to the
upper & lower bound.
If you have devices with very different temperature ratings (e.g. CPU vs.
mechanical hard drives), you should specify correction values to equalize
their temperature ranges, or better: use complex mode.
In complex mode, the upper and lower bounds of each fan level are specified
for each sensor individually.
Thinkfan then switches to the next fan level if one of the upper bounds is
reached, and to the previous fan level if all temperatures have dropped below
their respective lower bounds.

.SH THERMAL SENSORS
Multiple sensor keywords can be combined in one config file, but note that the
ordering is significant with respect to the upper and lower fan level bounds if
you use \fIcomplex mode\fR.
I.e. if
.B /proc/acpi/ibm/thermal
contains 16 temperatures and you specify an
.B hwmon
sensor after the
.B tp_thermal
statement, the
.B hwmon
sensor will be the 17th temperature.
.
After each sensor path, an optional
.I correction-value
can be specified.
This value (can be negative) is always added to the temperature reading from
that sensor.
Correction values should be specified if you use
.B Simple Mode
with components that have a different temperature rating, like hard disks and
CPUs.
Note though that
.B Complex Mode
is generally the better solution since it gives you full control over fan
levels and temperature ranges for each sensor, instead of just adding a fixed
value to equalize temperature ranges.

.TP
.BI "tp_thermal /proc/acpi/ibm/thermal" " \fR[\fB (\fIcorrection-value \fR...\fB) \fR]\fP"
Use the thermal sensors provided by the
.B thinkpad_acpi
kernel module on older thinkpads. These normally reside in
.B /proc/acpi/ibm/thermal,
so this keyword will hardly be used with other paths.
This file usually contains 8-16 temperatures, some of which may be
reserved for removable hardware or completely unused. Unused temperature slots
always contain the value -128. Since this file contains all temperatures the
.B thinkpad_acpi
module knows about, there cannot be more than one
.B tp_thermal
statement in a config file.

.TP
.BI hwmon " sysfs-path \fR[ \fB(\fIcorrection-value\fB) \fR]\fP"
Use a standard hwmon temperature input that may be provided by all kinds of
kernel drivers.
.I sysfs-path
is usually a file named \[oq]temp*_input\[cq], somewhere under /sys, so you
can search for them e.g. with \[oq]find /sys -type f -name "temp*_input"\[cq].
Each of these files contains one temperature, so you need to add a
.B hwmon
statement for each device whose temperature you wish to control.

.TP
.BI atasmart " device-path \fR[ \fB(\fIcorrection-value\fB) \fR]\fP"
NOTE: only available if thinkfan was compiled with USE_ATASMART enabled.
.
.IP
Read the temperature directly from a hard disk using S.M.A.R.T. See also the
.B -d
option in
.BR thinkfan (1)
that prevents thinkfan from waking up sleeping (mechanical) disks to read
their temperature.

.TP
.BI nv_thermal " pci-bus-id \fR[ \fB(\fIcorrection-value\fB) \fR]\fP"
NOTE: only available if thinkfan was compiled with USE_NVML enabled.
.
.IP
Read the temperature of an nVidia graphics card from the proprietary nVidia
driver. This does not work with the open-source Nouveau driver, it depends
specifially on libnvidia-ml.so that is usually installed with the binary nVidia
driver.
The correct
.I pci-bus-id
can be retrieved using e.g. lspci with: \[oq]lspci | grep -i vga\[cq].
Most open-source graphics drivers (radeon, nouveau, possibly others too) can
instead be used with the
.B hwmon
keyword described above.

.SH FANS
Currently, thinkfan can control only one fan at a time.
In theory, you can run multiple instances of the program simultaneously (with
multiple config files) to control multiple fans, but that requires enabling
DANGEROUS mode and will likely break most init scripts.
It is an error to have more than one fan statement per config file.

.TP
.B tp_fan /proc/acpi/ibm/fan
Use the fan control provided by the
.B thinkpad_acpi
kernel module, which needs to be loaded with the option
.BR fan_control=1 .
The path is defined by the
.B thinkpad_acpi
kernel module and will hardly change. Besides the fan levels ranging from 0 to
7, it also supports the
.B disengaged
and
.B auto
modes.
.
.IP
The
.B auto
mode should delegate fan control to the firmware, so it can be regarded as a
\[oq]default\[cq] mode that does not change the fan behavior.
This is useful for example if you only want to change the fan behavior at high
and/or low temperatures.
.
.IP
The
.B disengaged
or
.B full-speed
mode effectively disables the fan RPM limiter.
Fan speed will slowly ramp up until the fan uses the maximum electrical power
available from the embedded controller. Use this only to prevent potentially
destructive overheating, since it runs the fan outside of specifications and
wears its bearings down quickly.

.TP
.BI pwm_fan " sysfs-path"
Control a sysfs PWM fan.
Many hwmon drivers that provide a \[oq]temp*_input\[cq] file also allow fan control,
although there may also be drivers that are specific to either temperature
reading or fan control.
You can search for a PWM control file e.g. with \[oq]find /sys -type f -name
"pwm?"\[cq].
Note that with PWM, fan levels usually range from 0 to 255, although besides a
file like \fBpwm1\fR there may also be \fBpwm1_min\fR and \fBpwm1_max\fR that specify
different (soft or recommended?) limits for a particular fan.


.SH FAN LEVELS
Defining the fan levels is the meat of the config file. Here you make use of
your previously defined temperature inputs to set the lower and upper bounds
for the fan speeds.
You cannot mix simple fan levels with complex fan levels.
The general syntax of a simple fan level is:
.RS
.PP
\fB( \fIfan-level \fR[\fB,\fR] \fIlower-bound \fR[\fB,\fR] \fIupper-bound\fB )
.RE
.PP
The
.I fan-level
is either a numeric value (0-7 or 0-255, depending on whether a
.B tp_fan
or a
.B pwm_fan
is used) or a string enclosed in double quotes.
When a
.B tp_fan
is used, specifying
.B 0
has the same effect as specifying \fB"level 0\fR".
In addition to the numeric fan levels,
.B tp_fan
also supports \fB"level auto"\fR and \fB"level disengaged"\fR or \fB"level
full-speed"\fR.
See above for an explanation of what these mean.
The format of
.I lower-bound
and
.I upper-bound
depends on whether you want to use
.B Simple Mode
or
.B Complex Mode.


.SS Simple Mode
In simple mode, the
.I lower-bound
and
.I upper-bound
of a fan level are each specified as a single temperature value.
Both are compared only to the highest temperature found in all of the
configured thermal sensors.
Using this mode of operation makes sense e.g. if all temperature readings come
from the on-DIE thermal sensors of a multicore processor.
The fan speed will affect all of these temperatures in the same way because
they share a single thermal connection to the heatsink, so it makes sense to
ignore all but the highest of these temperatures.
As a rule of thumb, if your thermal sensors cover multiple devices you should
use Complex Mode, or at least specify correction values to account for
different temperature ratings.

.SS Complex Mode
In complex mode, both the
.I lower-bound
and
.I upper-bound
are lists of temperatures, the length of which must match the number of
temperature readings thinkfan knows about.
Each bound must be enclosed in braces, with individual values separated by
commas or spaces, so the specific syntax of a complex mode fan level is:
.RS
.PP
.nf
.B "{ \fIfan-level"
.B "    ( \fIlower-1 \fR[\fIlower-2\fR ...] \fB)"
.B "    ( \fIupper-1 \fR[\fIupper-2\fR ...] \fB)"
.B "}"
.fi
.RE
.PP
The optional commas have been omitted here for readability, and the curly
braces are interchangeable with round braces.
Note that it is not possible to mix simple fan levels with complex fan levels.
.P
Complex mode is generally the preferred mode of operation since it allows you
to specify precisely what the fan should to to keep each component within its
specified temperature range.


.SH SEE ALSO
.BR thinkfan (1)
.P
Example configs shipped with the source distribution, also available at
.IR https://github.com/vmatare/thinkfan/tree/master/examples .

