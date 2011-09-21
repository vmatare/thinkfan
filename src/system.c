/********************************************************************
 * system.c: Anything that interfaces with the operating system
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
 *
 * This file contains all functions that are specific to dealing with
 * either /sys/class/hwmon or the /proc/acpi/ibm interface. They are
 * referenced in the main program via function pointers.
 *
 * I know there's a lot of code redundancy in here, but that's expected
 * to save us some memory access in the main loop.
 * ******************************************************************/
#include "globaldefs.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include "message.h"
#include "system.h"
#include "parser.h"

const char temperatures[] = "temperatures:";

#define sensor_file_ibm \
		int i, ibm_temp; \
		ssize_t r; \
		char *input; \
		input = rbuf; \
		if (unlikely(((ibm_temp = open(IBM_TEMP, O_RDONLY)) < 0) \
				|| ((r = read(ibm_temp, rbuf, 128)) < 14) \
				|| (close(ibm_temp) < 0))) { \
			report(LOG_ERR, LOG_ERR, IBM_TEMP ": %s\n", strerror(errno)); \
			errcnt |= ERR_T_GET; \
			return 0; \
		} \
		rbuf[r] = 0;

int count_temps_ibm() {
	int *tmp;
	sensor_file_ibm

	skip_space(&input);
	i = 0;
	if(likely(parse_keyword(&input, temperatures) != NULL))
		for (i = 0; (tmp = parse_int(&input)); i++) free(tmp);
	return i;
}

/*******************************************************************
 * get_temp_ibm reads temperatures from /proc/acpi/ibm/thermal and
 * returns the number of temperatures read.
 *******************************************************************/
int get_temps_ibm() {

	long int tmp;
	char *s_input;
	sensor_file_ibm

	skip_space(&input);
	if (likely(parse_keyword(&input, temperatures) != NULL)) {
		i = 0;
		tmax = -128;
		while(*(s_input = input) && (
				(tmp = strtol(input, &input, 0))
				 || (input > s_input) )) {
			if (tmp < INT_MIN || tmp > INT_MAX) {
				errcnt |= ERR_T_GET;
				return i;
			}
			if (tmp > tmax) {
				b_tmax = temps + i;
				tmax = tmp;
			}
			temps[i] = (int)tmp + config->sensors->bias[i];
			i++;
		}

		if (unlikely(i < 2)) {
			report(LOG_ERR, LOG_ERR, MSG_ERR_T_GET);
			errcnt |= ERR_T_GET;
		}
	}
	else {
		report(LOG_ERR, LOG_ERR, MSG_ERR_T_PARSE(rbuf));
		errcnt |= ERR_T_GET;
	}
	return i;
}

/***********************************************************
 * Set fan speed (IBM interface).
 ***********************************************************/
void setfan_ibm() {
	int ibm_fan, l = strlen(cur_lvl);

	if (unlikely((ibm_fan = open(IBM_FAN, O_RDWR, O_TRUNC)) < 0)) {
		report(LOG_ERR, LOG_ERR, IBM_FAN ": %s\n", strerror(errno));
		errcnt |= ERR_FAN_SET;
	}
	else {
		if (unlikely(write(ibm_fan, cur_lvl, l) < l)) {
			report(LOG_ERR, LOG_ERR, MSG_ERR_FANCTRL);
			errcnt |= ERR_FAN_SET;
		}
		close(ibm_fan);
	}
}

/*********************************************************
 * Checks for fan_control support in thinkpad_acpi and
 * activates the fan watchdog.
 *********************************************************/
void init_fan_ibm() {
	char *line = NULL;
	size_t count = 0;
	FILE *ibm_fan;
	int module_valid=0;

	if ((ibm_fan = fopen(IBM_FAN, "r+")) == NULL) {
		report(LOG_ERR, LOG_ERR, IBM_FAN ": %s\n"
				MSG_ERR_FANFILE_IBM, strerror(errno));
		errcnt |= ERR_FAN_SET;
		return;
	}
	while (getline(&line, &count, ibm_fan) != -1)
		if (!strncmp("commands:", line, 9)) module_valid = 1;
	if (!module_valid) {
		report(LOG_ERR, LOG_ERR, MSG_ERR_MODOPTS);
		errcnt |= ERR_FAN_SET;
	}
	fprintf(ibm_fan, "watchdog %d\n", watchdog_timeout);
	fclose(ibm_fan);
	free(line);
}

/*********************************************************
 * Restores automatic fan control.
 *********************************************************/
void uninit_fan_ibm() {
	FILE *fan;

	if ((fan = fopen(IBM_FAN, "r+")) == NULL) {
		report(LOG_ERR, LOG_ERR, IBM_FAN ": %s\n", strerror(errno));
		errcnt |= ERR_FAN_SET;
	}
	else {
		fprintf(fan, "level auto\n");
		fclose(fan);
	}
}

/****************************************************************
 * Set the fan to disengaged mode for a specific duration <= 1s
 * to work-around the pulsating-fan problem.
 ****************************************************************/
void disengage() {
	int ibm_fan;
	if (unlikely((ibm_fan = open(IBM_FAN, O_RDWR)) < 0)) {
		report(LOG_ERR, LOG_ERR, IBM_FAN ": %s\n", strerror(errno));
		errcnt |= ERR_FAN_SET;
		return;
	}
	else {
		if (write(ibm_fan, "level disengaged", 16) < 16) {
			report(LOG_ERR, LOG_ERR, IBM_FAN ": %s\n", strerror(errno));
			errcnt |= ERR_FAN_SET;
		}
		close(ibm_fan);
	}
	if (usleep(depulse)) {
		report(LOG_ERR, LOG_ERR, "nanosleep(): %s\n", strerror(errno));
		errcnt |= ERR_FAN_SET;
	}
}

int depulse_and_get_temps_ibm() {
	disengage();
	config->setfan();
	return get_temps_ibm();
}
int depulse_and_get_temps_sysfs() {
	disengage();
	config->setfan();
	return get_temps_sysfs();
}

/****************************************************************
 * get_temps_sysfs() reads the temperature from all files that
 * were specified as "sensor ..." in the config file and stores
 * them in the global variable "temps".
 ****************************************************************/
int get_temps_sysfs() {
	int i, num, fd;
	long int tmp;
	char buf[8];
	char *input = buf, *end;

	tmax = -128;
	for (i = 0; i < config->num_sensors; i++) {
		if (unlikely((fd = open(config->sensors[i].path, O_RDONLY)) == -1
				|| (num = read(fd, &buf, 7)) == -1
				|| close(fd) < 0)) {
			report(LOG_ERR, LOG_ERR, "%s: %s\n", config->sensors[i].path,
					strerror(errno));
			errcnt |= ERR_T_GET;
			return i;
		}
		tmp = strtol(input, &end, 0);
		if (tmp < INT_MIN || tmp > INT_MAX) {
			errcnt |= ERR_T_GET;
			return i;
		}
		tmp /= 1000;
		if (tmp > tmax) {
			tmax = (int)tmp;
			b_tmax = temps + i;
		}
		temps[i] = config->sensors[i].bias[0] + (int)tmp;
	}
	return i;
}

/***********************************************************
 * Set fan speed (sysfs interface).
 ***********************************************************/
void setfan_sysfs() {
	int fan, l = strlen(cur_lvl);

	if (unlikely((fan = open(config->fan, O_WRONLY)) < 0)) {
		report(LOG_ERR, LOG_ERR, "%s: %s\n", config->fan, strerror(errno));
		errcnt++;
	}
	else {
		if (unlikely(write(fan, cur_lvl, l) < l)) {
			report(LOG_ERR, LOG_ERR, MSG_ERR_FANCTRL);
			errcnt++;
		}
		close(fan);
	}
}

/***********************************************************
 * Suspend/Resume-safe way of setting fan speed
 ***********************************************************/
void setfan_sysfs_safe() {
	init_fan_sysfs();
	setfan_sysfs();
}

void init_fan_sysfs_once() {
	preinit_fan_sysfs();
	init_fan_sysfs();
}


/***********************************************************
 * Store old pwm_enable value to cleanly reset it when exiting
 ***********************************************************/
void preinit_fan_sysfs() {
	if (oldpwm) return;

	char *fan_enable = (char *) malloc((strlen(config->fan) + 8) * sizeof(char));
	FILE *fan = NULL;
	size_t s = 0;
	ssize_t r = 0;

	strcpy(fan_enable, config->fan);
	strcat(fan_enable, "_enable");

	if ((fan = fopen(fan_enable, "r")) == NULL) {
		report(LOG_ERR, LOG_ERR, "%s: %s\n", fan_enable, strerror(errno));
		free(fan_enable);
		errcnt |= ERR_FAN_INIT;
	}
	else {
		if ((r = getline(&oldpwm, &s, fan)) < 2)
			report(LOG_ERR, LOG_ERR, "%s: %s\n", fan_enable, strerror(errno));
		if (r < 2) {
			report(LOG_ERR, LOG_ERR, MSG_ERR_FAN_INIT);
			errcnt |= ERR_FAN_INIT;
		}
		fclose(fan);
		free(fan_enable);
	}
}

/*********************************************************
 * This activates userspace PWM control.
 *********************************************************/
void init_fan_sysfs() {
	int fd;
	char *fan_enable = (char *) malloc((strlen(config->fan) + 8) * sizeof(char));
	ssize_t r;

	strcpy(fan_enable, config->fan);
	strcat(fan_enable, "_enable");

	if ((fd = open(fan_enable, O_WRONLY)) < 0) {
		report(LOG_ERR, LOG_ERR, "%s: %s\n", fan_enable, strerror(errno));
		free(fan_enable);
		errcnt |= ERR_FAN_INIT;
		goto fail;
	}
	if ((r = write(fd, "1\n", 2)) < 2)
		report(LOG_ERR, LOG_ERR, "%s: %s\n", fan_enable, strerror(errno));
	if (r < 2) {
		report(LOG_ERR, LOG_ERR, MSG_ERR_FAN_INIT);
		errcnt |= ERR_FAN_INIT;
	}
fail:
	close(fd);
	free(fan_enable);
}

/*********************************************************
 * Restore previous fan control mode.
 *********************************************************/
void uninit_fan_sysfs() {
	FILE *fan;

	if (oldpwm) {
		if ((fan = fopen(config->fan, "r+")) == NULL) {
			report(LOG_ERR, LOG_ERR, "%s: %s\n", config->fan, strerror(errno));
			errcnt++;
		}
		else {
			fprintf(fan, "%s", oldpwm);
			fclose(fan);
		}
		free(oldpwm);
		oldpwm = NULL;
	}
}

