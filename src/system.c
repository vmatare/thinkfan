/********************************************************************
 * system.c: Anything that interfaces with the operating system
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

/*******************************************************************
 * get_temp_ibm reads temperatures from /proc/acpi/ibm/thermal and
 * returns the highest one found.
 *******************************************************************/
int get_temp_ibm() {
	int i=0, res, retval=0, ibm_temp, *tmp;
	ssize_t r;
	char *input;
	input = rbuf;

	if (unlikely(((ibm_temp = open(IBM_TEMP, O_RDONLY)) < 0)
			|| ((r = read(ibm_temp, rbuf, 128)) < 14)
			|| (close(ibm_temp) < 0))) {
		report(LOG_ERR, LOG_ERR, IBM_TEMP ": %s", strerror(errno));
		errcnt++;
		return ERR_T_GET;
	}
	rbuf[r] = 0;

	skip_space(&input);
	if (likely(parse_keyword(&input, temperatures) != NULL)) {
		for (i = 0; ((tmp = parse_int(&input)) && (i < 16)); i++) {
			res = *tmp + config->sensors->bias[i];
			if (res > retval) retval = res;
			free(tmp);
		}
		if (unlikely(i < 2)) {
			report(LOG_ERR, LOG_ERR, MSG_ERR_T_GET);
			errcnt++;
			retval = ERR_T_GET;
		}
	}
	else {
		report(LOG_ERR, LOG_ERR, MSG_ERR_T_PARSE(rbuf));
		errcnt++;
		retval = ERR_T_GET;
	}
	return retval;
}

/***********************************************************
 * Set fan speed (IBM interface).
 ***********************************************************/
void setfan_ibm() {
	int ibm_fan;
	char *buf = malloc(18 * sizeof(char));

	if (unlikely((ibm_fan = open(IBM_FAN, O_RDWR, O_TRUNC)) < 0)) {
		report(LOG_ERR, LOG_ERR, IBM_FAN ": %s", strerror(errno));
		errcnt++;
	}
	else {
		if (unlikely(cur_lvl == INT_MIN)) strcpy(buf, "level disengaged\n");
		else snprintf(buf, 10, "level %d\n", cur_lvl);
		if (unlikely(write(ibm_fan, buf, strlen(buf)) < 8)) {
			report(LOG_ERR, LOG_ERR, MSG_ERR_FANCTRL);
			errcnt++;
		}
		close(ibm_fan);
	}
	free(buf);
}

/*********************************************************
 * Checks for fan_control support in thinkpad_acpi and
 * activates the fan watchdog.
 *********************************************************/
int init_fan_ibm() {
	char *line = NULL;
	size_t count = 0;
	FILE *ibm_fan;
	int module_valid=0;

	if ((ibm_fan = fopen(IBM_FAN, "r+")) == NULL) {
		report(LOG_ERR, LOG_ERR, IBM_FAN ": %s\n"
				MSG_ERR_FANFILE_IBM, strerror(errno));
		errcnt++;
		return ERR_FAN_INIT;
	}
	while (getline(&line, &count, ibm_fan) != -1)
		if (!strncmp("commands:", line, 9)) module_valid = 1;
	if (!module_valid) {
		report(LOG_ERR, LOG_ERR, MSG_ERR_MODOPTS);
		errcnt++;
		return ERR_FAN_INIT;
	}
	fprintf(ibm_fan, "watchdog %d\n", watchdog_timeout);
	fclose(ibm_fan);
	free(line);
	return 0;
}

/*********************************************************
 * Restores automatic fan control.
 *********************************************************/
void uninit_fan_ibm() {
	FILE *fan;

	if ((fan = fopen(IBM_FAN, "r+")) == NULL) {
		report(LOG_ERR, LOG_ERR, IBM_FAN ": %s", strerror(errno));
		errcnt++;
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
		report(LOG_ERR, LOG_ERR, IBM_FAN ": %s", strerror(errno));
		errcnt++;
	}
	else {
		if (write(ibm_fan, "level disengaged", 16) < 16) {
			report(LOG_ERR, LOG_ERR, IBM_FAN ": %s", strerror(errno));
			errcnt++;
		}
		close(ibm_fan);
	}
	if (nanosleep(depulse, NULL)) {
		report(LOG_ERR, LOG_ERR, "nanosleep(): %s", strerror(errno));
		errcnt++;
	}
}

int depulse_and_get_temp_ibm() {
	if (cur_lvl >= DEPULSE_MIN_LVL && cur_lvl <= DEPULSE_MAX_LVL) {
		disengage();
		setfan_ibm();
	}
	return get_temp_ibm();
}
int depulse_and_get_temp_sysfs() {
	if (cur_lvl >= DEPULSE_MIN_LVL && cur_lvl <= DEPULSE_MAX_LVL) {
		disengage();
		setfan_sysfs();
	}
	return get_temp_sysfs();
}

/****************************************************************
 * get_temp_sysfs() reads the temperature from all files that
 * were specified as "sensor ..." in the config file and returns
 * the highest temperature.
 ****************************************************************/
int get_temp_sysfs() {
	int num, fd, idx = 0;
	long int rv = 0, tmp;
	char buf[7];
	char *endptr;
	while(idx < config->num_sensors) {
		if (unlikely((fd = open(config->sensors[idx].path, O_RDONLY)) == -1
				|| (num = read(fd, &buf, 6)) == -1
				|| close(fd) < 0)) {
			report(LOG_ERR, LOG_ERR, "%s: %s", config->sensors[idx].path,
					strerror(errno));
			errcnt++;
			return ERR_T_GET;
		}
		buf[num] = 0;
		tmp = config->sensors[idx].bias[0] + strtol(buf, &endptr, 10)/1000;
		if (tmp > rv) rv = tmp;
		idx++;
	}
	if (unlikely(rv < 1)) {
		report(LOG_ERR, LOG_ERR, MSG_ERR_T_GET);
		errcnt++;
	}
	return rv;
}

/***********************************************************
 * Set fan speed (sysfs interface).
 ***********************************************************/
void setfan_sysfs() {
	int fan, r;
	ssize_t ret;
	char *buf = malloc(5 * sizeof(char));
	memset(buf, 0, 5);

	if (unlikely((fan = open(config->fan, O_WRONLY)) < 0)) {
		report(LOG_ERR, LOG_ERR, "%s: %s", config->fan, strerror(errno));
		errcnt++;
	}
	else {
		r = snprintf(buf, 5, "%d\n", cur_lvl);
		ret = (int)r + write(fan, buf, 5);
		close(fan);
		if (unlikely(ret < 2)) {
			report(LOG_ERR, LOG_ERR, MSG_ERR_FANCTRL);
			errcnt++;
		}
	}
	free(buf);
}

/***********************************************************
 * Suspend/Resume-safe way of setting fan speed
 ***********************************************************/
void setfan_sysfs_safe() {
	if(!init_fan_sysfs()) setfan_sysfs();
}

int init_fan_sysfs_once() {
	int rv;
	if (!(rv = preinit_fan_sysfs())) return init_fan_sysfs();
	return rv;
}


/***********************************************************
 * Store old pwm_enable value to cleanly reset it when exiting
 ***********************************************************/
int preinit_fan_sysfs() {
	char *fan_enable = (char *) malloc((strlen(config->fan) + 8) * sizeof(char));
	FILE *fan = NULL;
	size_t s;
	ssize_t r = 0;

	strcpy(fan_enable, config->fan);
	strcat(fan_enable, "_enable");

	if ((fan = fopen(fan_enable, "r")) == NULL) {
		report(LOG_ERR, LOG_ERR, "%s: %s", fan_enable, strerror(errno));
		errcnt++;
		free(fan_enable);
		return ERR_FAN_INIT;
	}
	free(oldpwm);
	oldpwm = NULL;
	if ((r = getline(&oldpwm, &s, fan)) < 2)
		report(LOG_ERR, LOG_ERR, "%s: %s", fan_enable, strerror(errno));
	fclose(fan);
	free(fan_enable);
	if (r < 2) {
		errcnt++;
		report(LOG_ERR, LOG_ERR, MSG_ERR_FAN_INIT);
		return ERR_FAN_INIT;
	}
	return 0;
}

/*********************************************************
 * This activates userspace PWM control.
 *********************************************************/
int init_fan_sysfs() {
	int fd;
	char *fan_enable = (char *) malloc((strlen(config->fan) + 8) * sizeof(char));
	ssize_t r;

	strcpy(fan_enable, config->fan);
	strcat(fan_enable, "_enable");

	if ((fd = open(fan_enable, O_WRONLY)) < 0) {
		report(LOG_ERR, LOG_ERR, "%s: %s", fan_enable, strerror(errno));
		errcnt++;
		free(fan_enable);
		return ERR_FAN_INIT;
	}
	if ((r = write(fd, "1\n", 2)) < 2)
		report(LOG_ERR, LOG_ERR, "%s: %s", fan_enable, strerror(errno));
	close(fd);
	free(fan_enable);
	if (r < 2) {
		errcnt++;
		report(LOG_ERR, LOG_ERR, MSG_ERR_FAN_INIT);
		return ERR_FAN_INIT;
	}
	return 0;
}

/*********************************************************
 * Restore previous fan control mode.
 *********************************************************/
void uninit_fan_sysfs() {
	FILE *fan;

	if (oldpwm) {
		if ((fan = fopen(config->fan, "r+")) == NULL) {
			report(LOG_ERR, LOG_ERR, "%s: %s", config->fan, strerror(errno));
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

