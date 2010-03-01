/********************************************************************
 * message.h
 *
 * This work is licensed under a Creative Commons Attribution-Share Alike 3.0
 * United States License. See http://creativecommons.org/licenses/by-sa/3.0/us/
 * for details.
 *
 * This file is part of thinkfan. See thinkfan.c for further info.
 * ******************************************************************/

#ifndef MESSAGE_C
#define MESSAGE_C

#define showerr(cause) { \
	int lasterr = errno; \
	if (nodaemon) { \
		fputs(prefix, stderr); \
		fprintf(stderr, "%s: %s\n", cause, strerror(lasterr)); \
	} \
	syslog(LOG_ERR, "%s: %s\n", cause, strerror(lasterr)); \
	prefix = ""; \
}
#define message(loglevel, message) { \
	if (nodaemon && (!quiet || loglevel > LOG_WARNING)) { \
		fputs(prefix, stderr); \
		fprintf(stderr, message); \
	} \
	else syslog(loglevel, message); \
	prefix = ""; \
}

#define message_fg(loglevel, message) \
	if (!quiet || loglevel > LOG_WARNING) { \
		fputs(prefix, stderr); \
		fprintf(stderr, message); \
	} \
	prefix = "";

#define MSG_USAGE \
 "\nthinkfan " VERSION ": A minimalist fan control program\n" \
 "\nUsage: thinkfan [-hnqzD [-b BIAS] [-c CONFIG] [-s SECONDS] [-p [SECONDS]]]" \
 "\n -h  This help message" \
 "\n -s  Maximum cycle time (Integer. Default: 5)" \
 "\n -b  Floating point number (0 ~ 20) to control rising edge" \
 "\n     temperature biasing strength (see README). Default 5." \
 "\n -c  Load different configuration file (default: /etc/thinkfan.conf)" \
 "\n -n  Do not become a daemon and log to terminal & syslog" \
 "\n -q  Be quiet (no status info on terminal)" \
 "\n -z  Assume we don't have to worry about resuming when using the sysfs" \
 "\n     interface (see README!)" \
 "\n -p  Use the pulsing-fan workaround (for older Thinkpads). Takes an optional" \
 "\n     floating-point argument (0 ~ 10s) as depulsing duration. Default 0.5s." \
 "\n -D  DANGEROUS mode: Disable all sanity checks. May damage your " \
 "hardware!!\n\n"

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
#define MSG_WRN_SLEEPTIME_15 "WARNING: %d seconds of not realizing " \
 "rising temperatures may be dangerous!\n", sleeptime
#define MSG_WRN_SLEEPTIME_1 "A sleeptime of %d seconds doesn't make much " \
 "sense.\n", sleeptime
#define MSG_ERR_T_GET "Error getting temperature.\n"
#define MSG_ERR_MODOPTS \
 "Module thinkpad_acpi doesn't seem to support fan_control\n"
#define MSG_ERR_FANCTRL "Error writing to %s.\n", config->fan
#define MSG_ERR_FAN_INIT "Error initializing fan control.\n"
#define MSG_ERR_OPT_S "ERROR: option -s requires an int argument!\n"
#define MSG_ERR_OPT_B "ERROR: option -b requires a float argument!\n"
#define MSG_ERR_OPT_P "ERROR: invalid argument to option -p: %f\n", depulse_tmp
#define MSG_ERR_CONF_NOFILE "Refusing to run without usable config file." \
 " Please read the documentation.\n"
#define MSG_ERR_CONF_LOST "Lost configuration! This is a bug. Please " \
 "report this to the author.\n"
#define MSG_ERR_CONF_RELOAD "Error reloading config. Keeping old one.\n"
#define MSG_ERR_CONF_NOFAN "Could not find any fan speed settings in" \
	" the config file. Please read the documentation.\n"
#define MSG_ERR_CONF_LOWHIGH(file, num, line) "%s:%d: %sLOWER limit must be" \
	" smaller than HIGHER!\nReally, don't mess with this, it could trash your"\
	" hardware.\n", file, num, line
#define MSG_ERR_RUNNING PID_FILE " already exists. Either thinkfan is " \
	"already running, or it was killed by SIGKILL. If you're sure thinkfan" \
	" is not running, delete " PID_FILE " manually.\n"
#define MSG_ERR_CONF_FAN(file, num, line) "%s:%d: %sThinkfan can't use more " \
	"than one fan in this release.\n", file, num, line
#define MSG_WRN_SYSFS_SAFE "WARNING: Using safe but wasteful way of settin" \
	"g PWM value. Check README to know more.\n"
#define MSG_ERR_FANFILE_IBM "Error opening " IBM_FAN ". Is this a computer " \
	"really Thinkpad? Is the thinkpad_acpi module loaded? Are you running thi" \
	"nkfan with root privileges?\n"
#define MSG_ERR_CONF_MIX(file, num, line) "%s:%d: %sThinkfan can't use " \
	"sysfs sensors together with thinkpad_acpi sensors. Please choose one.\n" \
	, file, num, line
#define MSG_INF_DEPULSE(delay, time) "Disengaging the fan controller for " \
	"%.3f seconds every %d seconds\n", time, delay
#define MSG_ERR_T_PARSE(str) "Error parsing temperatures: %s\n", str
#define MSG_WRN_SENSOR_DEFAULT "WARNING: Using default temperature inputs in" \
	" /proc/acpi/ibm/thermal. This comes with a risk of overheating your " \
	"hard disk. Please check out the new example configurations to learn " \
	"how to prevent that.\n"
#define MSG_ERR_CONF_PARSE(file, num, line) "%s:%d: %sSyntax error.\n", file, \
	num, line

#endif
