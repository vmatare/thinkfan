/************************************************************************
 * globaldefs.h: Stuff that's needed by all objects
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
 ************************************************************************/
#ifndef GLOBALDEFS_H
#define GLOBALDEFS_H

#define _GNU_SOURCE

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>

#define VERSION "0.8"

#define ERR_T_GET          1
#define ERR_FAN_INIT       1<<1
#define ERR_CONF_NOFILE    1<<2
#define ERR_CONF_LOST      1<<3
#define ERR_CONF_RELOAD    1<<4
#define ERR_PIDFILE        1<<5
#define ERR_FORK           1<<6
#define ERR_CONF_MIX       1<<7
#define ERR_MALLOC         1<<8
#define ERR_CONF_LOWHIGH   1<<9
#define ERR_CONF_LVLORDER  1<<10
#define ERR_CONF_ORDERLOW  1<<11
#define ERR_CONF_ORDERHIGH 1<<12
#define ERR_CONF_OVERLAP   1<<13
#define ERR_CONF_LVL0      1<<14
#define ERR_FAN_SET        1<<15
#define ERR_CONF_LIMIT     1<<16
#define WRN_CONF_INTMIN_LVL 1<<17
#define ERR_CONF_LVLFORMAT 1<<18
#define ERR_CONF_LIMITLEN  1<<19

#ifndef DUMMYRUN
#define PID_FILE "/var/run/thinkfan.pid"
#define IBM_TEMP "/proc/acpi/ibm/thermal"
#define IBM_FAN "/proc/acpi/ibm/fan"
#else
#define PID_FILE "/tmp/thinkfan.pid"
#define IBM_TEMP "/tmp/thermal"
#define IBM_FAN "/tmp/fan"
#endif //DUMMYRUN

// Stolen from the gurus
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

struct limit {
	char *level; // "level x" representation for /proc/acpi/ibm/fan.
	char *sysfslevel; // numeric representation for /sys/class/hwmon
	int nlevel;   // A numeric interpretation of the level
	int *low;   // int array specifying the LOWER limit, terminated by INT_MIN
	int *high;  // dito for UPPER limit.
};

struct sensor {
	char *path;
	int bias[16];
};

struct tf_config {
	struct sensor *sensors;
	int num_sensors;
	char *fan;
	struct limit *limits;
	int num_limits;
	int limit_len;
	int (*get_temps)();
	void (*setfan)();
	void (*init_fan)();
	void (*uninit_fan)();
	int (*lvl_up)();
	int (*lvl_down)();
	int (*cmp_lvl)();
};


struct tf_config *config;
unsigned long int errcnt;
int *temps, tmax, last_tmax, lvl_idx, *b_tmax, line_count;
unsigned int chk_sanity, watchdog_timeout, num_temps;
char *config_file, *prefix, *rbuf,
	errmsg[1024],
	quiet, nodaemon, resume_is_safe,
	*oldpwm; // old contents of pwm*_enable, used for uninit_fan()
float bias_level, depulse_tmp;
useconds_t depulse;
#define FALSE 0
#define TRUE !FALSE


#endif
