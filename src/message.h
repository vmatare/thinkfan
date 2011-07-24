/********************************************************************
 * message.h
 *
 * This file is part of thinkfan.

 * thinkfan is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * thinkfan is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with thinkfan.  If not, see <http://www.gnu.org/licenses/>.
 ********************************************************************/

#ifndef MESSAGE_H
#define MESSAGE_H

void report(int nlevel, int dlevel, char *format, ...);

#define MSG_USAGE \
 "\nthinkfan " VERSION ": A minimalist fan control program\n" \
 "\nUsage: thinkfan [-hnqzD [-b BIAS] [-c CONFIG] [-s SECONDS] [-p [SECONDS]]]" \
 "\n -h  This help message" \
 "\n -s  Maximum cycle time in seconds (Integer. Default: 5)" \
 "\n -b  Floating point number (0 ~ 20) to control rising edge" \
 "\n     temperature biasing strength (see README). Default: 5.0" \
 "\n -c  Load different configuration file (default: /etc/thinkfan.conf)" \
 "\n -n  Do not become a daemon and log to terminal & syslog" \
 "\n -q  Be quiet (no status info on terminal)" \
 "\n -z  Assume we don't have to worry about resuming when using the sysfs" \
 "\n     interface (see README!)" \
 "\n -p  Use the pulsing-fan workaround (for older Thinkpads). Takes an optional" \
 "\n     floating-point argument (0 ~ 10s) as depulsing duration. Default 0.5s." \
 "\n -D  DANGEROUS mode: Disable all sanity checks. May damage your " \
 "hardware!!\n\n"

#define MSG_FILE_HDR(file, num, line) "%s:%d:%s\n", file, num, line

#define MSG_DBG_T_STAT "sleeptime=%d, temp=%d, last_temp=%d, biased_temp=%d" \
 " -> level=%d\n", st, temp, last_temp, b_temp, cur_lvl
#define MSG_DBG_CONF_RELOAD "Received SIGHUP: reloading config...\n"

#define MSG_INF_SANITY "Sanity checks are on. Exiting.\n"
#define MSG_INF_INSANITY "Sanity checks are off. Continuing.\n"
#define MSG_INF_CONFIG \
 "Config as read from %s:\nFan level\tLow\tHigh\n", config_file
#define MSG_INF_CONF_ITEM(level, low, high) " %d\t\t%d\t%d\n", level, low, high
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
	" /proc/acpi/ibm/thermal.\n"
#define MSG_WRN_CONF_NOBIAS(t) "WARNING: You have not provided any correction" \
	" values for any sensor, and your fan will only start at %d °C. This can " \
	"be dangerous for your hard drive.\n", t

#define MSG_ERR_T_GET "Error getting temperature.\n"
#define MSG_ERR_MODOPTS \
 "Module thinkpad_acpi doesn't seem to support fan_control\n"
#define MSG_ERR_FANCTRL "Error writing to %s: %s\n", config->fan, strerror(errno)
#define MSG_ERR_FAN_INIT "Error initializing fan control.\n"
#define MSG_ERR_OPT_S "ERROR: option -s requires an int argument!\n"
#define MSG_ERR_OPT_B "ERROR: option -b requires a float argument!\n"
#define MSG_ERR_OPT_P "ERROR: invalid argument to option -p: %f\n", depulse_tmp

#define MSG_ERR_CONF_NOFILE "Refusing to run without usable config file." \
 " Please read AND UNDERSTAND the documentation!\n"
#define MSG_ERR_CONF_LOST "Lost configuration! This is a bug. Please " \
 "report this to the author.\n"
#define MSG_ERR_CONF_RELOAD "Error reloading config. Keeping old one.\n"
#define MSG_ERR_CONF_NOFAN "Could not find any fan speed settings in" \
	" the config file. Please read AND UNDERSTAND the documentation!\n"
#define MSG_ERR_CONF_LOWHIGH "LOWER limit must be smaller than HIGHER! " \
	"Really, don't mess with this, it could trash your hardware.\n"
#define MSG_ERR_CONF_OVERLAP "LOWER limit doesn't overlap with previous UPPER" \
	" limit.\n"
#define MSG_ERR_CONF_FAN "Thinkfan can't use more than one fan.\n"
#define MSG_ERR_CONF_MIX "Thinkfan can't use sysfs sensors together with " \
	"thinkpad_acpi sensors. Please choose one.\n"
#define MSG_ERR_CONF_LEVEL "Fan levels are not ordered correctly.\n"
#define MSG_ERR_CONF_PARSE "Syntax error.\n"
#define MSG_ERR_CONF_LVL0 "The LOWER limit of the first fan level must be 0!\n"

#define MSG_ERR_RUNNING PID_FILE " already exists. Either thinkfan is " \
	"already running, or it was killed by SIGKILL. If you're sure thinkfan" \
	" is not running, delete " PID_FILE " manually.\n"
#define MSG_ERR_FANFILE_IBM "Error opening " IBM_FAN ". Is this a computer " \
	"really Thinkpad? Is the thinkpad_acpi module loaded? Are you running thi" \
	"nkfan with root privileges?\n"
#define MSG_ERR_T_PARSE(str) "Error parsing temperatures: %s\n", str

#endif
