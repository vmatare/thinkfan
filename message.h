/********************************************************************
 * message.h
 *
 * this file is part of thinkfan. See thinkfan.c for further information.
 *
 * thinkfan is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * thinkfan is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with thinkfan.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ******************************************************************/

#ifndef MESSAGE_H
#define MESSAGE_H

void report(int nlevel, int dlevel, char *format, ...);

#ifdef USE_ATASMART
#define DND_DISK_HELP \
 "\n -d  Don't read S.M.A.R.T. temperature from sleeping disks"
#else
#define DND_DISK_HELP ""
#endif

#define MSG_USAGE \
 "\nthinkfan " VERSION ": A minimalist fan control program\n" \
 "\nUsage: thinkfan [-hnqzD [-b BIAS] [-c CONFIG] [-s SECONDS] [-p [SECONDS]]]" \
 "\n -h  This help message" \
 "\n -s  Maximum cycle time in seconds (Integer. Default: 5)" \
 "\n -b  Floating point number (-10 to 30) to control rising temperature" \
 "\n     exaggeration (see README). Default: 5.0" \
 "\n -c  Load different configuration file (default: /etc/thinkfan.conf)" \
 "\n -n  Do not become a daemon and log to terminal" \
 "\n -q  Be quiet (report only important events)" \
 "\n -z  Assume we don't have to worry about resuming when using the sysfs" \
 "\n     interface (see README!)" \
 "\n -p  Use the pulsing-fan workaround (for older Thinkpads). Takes an optional" \
 "\n     floating-point argument (0 ~ 10s) as depulsing duration. Default 0.5s." \
 DND_DISK_HELP \
 "\n -D  DANGEROUS mode: Disable all sanity checks. May result in undefined"\
 "\n     behaviour!\n\n"

#define MSG_FILE_HDR(file, line) "%s:%d:%s\n", file, line_count, line

#define MSG_DBG_T_STAT "sleeptime=%d, tmax=%d, last_tmax=%d, biased_tmax=%d" \
 " -> fan=\"%s\"\n", tmp_sleeptime, tmax, last_tmax, *b_tmax, config->limits[lvl_idx].level
#define MSG_DBG_CONF_RELOAD "Received SIGHUP: reloading config...\n"

#define MSG_INF_SANITY "Sanity checks are on. Exiting.\n"
#define MSG_INF_INSANITY "Sanity checks are off. Continuing.\n"
#define MSG_INF_CONFIG \
 "Config as read from %s:\nFan level\tLow\tHigh\n", config_file
#define MSG_INF_CONF_ITEM(level, low, high) " %s\t\t%d\t%d\n", level, low, high
#define MSG_INF_TERM \
 "Cleaning up and resetting fan control.\n"
#define MSG_INF_DEPULSE(delay, time) "Disengaging the fan controller for " \
	"%.3f seconds every %d seconds\n", time, delay

#define MSG_WRN_SLEEPTIME_15 "WARNING: %d seconds of not realizing " \
 "rising temperatures may be dangerous!\n", sleeptime
#define MSG_WRN_SLEEPTIME_1 "A sleeptime of %d seconds doesn't make much " \
 "sense.\n", sleeptime
#define MSG_WRN_SYSFS_SAFE "WARNING: Using safe but wasteful way of settin" \
	"g PWM value. Check README to know more.\n"
#define MSG_WRN_SENSOR_DEFAULT "WARNING: Using default temperature inputs in" \
	" " IBM_TEMP ".\n"
#define MSG_WRN_FAN_DEFAULT "WARNING: Using default fan control in" \
	" " IBM_FAN ".\n"
#define MSG_WRN_CONF_NOBIAS(t) "WARNING: You're using simple temperature limits" \
	" without correction values, and your fan will only start at %d °C. This can " \
	"be dangerous for your hard drive.\n", t
#define MSG_WRN_LVL_DISENGAGED "WARNING: You're using INT_MIN as a fan level." \
	" Fan levels are strings now, so please replace it by \"level disengaged\"."
#define MSG_WRN_NUM_TEMPS(n, i) "WARNING: You have %d sensors but your temper" \
	"ature limits only have %d entries. Excess sensors will be ignored.\n", n, i
#define MSG_WRN_SENSOR_DEPRECATED "WARNING: The `sensor' keyword is deprecat" \
	"ed. Please use the `hwmon' or `tp_thermal' keywords instead!\n"
#define MSG_WRN_FAN_DEPRECATED "WARNING: Guessing the fan type from the path" \
	" argument to the `fan' keyword is deprecated. Please use `tp_fan' or" \
	" `pwm_fan' to make things clear.\n"

#define MSG_ERR_T_GET "%s: Error getting temperature.\n", __func__
#define MSG_ERR_T_GARBAGE "%s: Trailing garbage after temperature!\n"
#define MSG_ERR_T_INVALID "%s: Invalid temperature: %d"
#define MSG_ERR_MODOPTS \
 "Module thinkpad_acpi doesn't seem to support fan_control\n"
#define MSG_ERR_FANCTRL "%s: Error writing to %s: %s\n", __func__, config->fan, strerror(errno)
#define MSG_ERR_FAN_INIT "%s: Error initializing fan control.\n", __func__
#define MSG_ERR_OPT_S "ERROR: option -s requires an int argument!\n"
#define MSG_ERR_OPT_B "ERROR: bias must be between -10 and 30!\n"
#define MSG_ERR_OPT_P "ERROR: invalid argument to option -p: %f\n", depulse_tmp

#define MSG_ERR_CONF_NOFILE "Refusing to run without usable config file!\n"
#define MSG_ERR_CONF_LOST "%s: Lost configuration! This is a bug. Please " \
 "report this to the author.\n", __func__
#define MSG_ERR_CONF_RELOAD "Error reloading config. Keeping old one.\n"
#define MSG_ERR_CONF_NOFAN "Could not find any fan speed settings in" \
	" the config file. Please read AND UNDERSTAND the documentation!\n"
#define MSG_ERR_CONF_LOWHIGH "Your LOWER limit is not lesser than your " \
	"UPPER limit. That doesn't make sense.\n"
#define MSG_ERR_CONF_OVERLAP "LOWER limit doesn't overlap with previous UPPER" \
	" limit.\n"
#define MSG_ERR_CONF_FAN "Thinkfan can't use more than one fan.\n"
#define MSG_ERR_CONF_LVLORDER "Fan levels are not ordered correctly.\n"
#define MSG_ERR_CONF_PARSE "Syntax error.\n"
#define MSG_ERR_CONF_LVL0 "The LOWER limit of the first fan level cannot con" \
	"tain any values greater than 0!\n"
#define MSG_ERR_CONF_LVLFORMAT "Invalid fan level string. This check can" \
	" be disabled by using DANGEROUS mode.\n"

#define MSG_ERR_RUNNING PID_FILE " already exists. Either thinkfan is " \
	"already running, or it was killed by SIGKILL. If you're sure thinkfan" \
	" is not running, delete " PID_FILE " manually.\n"
#define MSG_ERR_FANFILE_IBM "Error opening " IBM_FAN ". Is this a computer " \
	"really Thinkpad? Is the thinkpad_acpi module loaded? Are you running thi" \
	"nkfan with root privileges?\n"
#define MSG_ERR_T_PARSE(str) "Error parsing temperatures: %s\n", str
#define MSG_ERR_LCOUNT "The number of limits must either be 1 or equal to the" \
	" number of temperatures.\n"
#define MSG_ERR_LONG_LIMIT "You have configured more temperature limits " \
	"than sensors. That doesn't make sense.\n"
#define MSG_ERR_LIMITLEN "Inconsistent limit length.\n"
#define MSG_ERR_TEMP_COUNT "Your config requires at least %d temperatures, " \
	"but only %d temperatures were found.\n"
#define MSG_ALERT_SENSOR "A sensor has vanished! Exiting since there's no " \
	"safe way of handling this.\n"

#endif
