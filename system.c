/********************************************************************
 * system.c: Anything that interfaces with the operating system
 *
 * This work is licensed under a Creative Commons Attribution-Share Alike 3.0
 * United States License. See http://creativecommons.org/licenses/by-sa/3.0/us/
 * for details.
 *
 * This file contains all functions that are specific to dealing with
 * either /sys/class/hwmon or the /proc/acpi/ibm interface. They are
 * referenced in the main program via function pointers.
 *
 * This file is part of thinkfan. See thinkfan.c for further info.
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

/*******************************************************************
 * get_temp_ibm reads temperatures from /proc/acpi/ibm/thermal and
 * returns the highest one found.
 *******************************************************************/
int get_temp_ibm() {
	int i=0, res, retval=0;
	int *t = malloc(16 * sizeof(int));
	int ibm_temp;
	ssize_t r;
	char buf[128];

	memset(t, 0, 16 * sizeof(int));
	if (unlikely(((ibm_temp = open(IBM_TEMP, O_RDONLY)) < 0)
			|| ((r = read(ibm_temp, &buf, 128)) < 14)
			|| (close(ibm_temp) < 0))) {
		showerr(IBM_TEMP);
		free(t);
		errcnt++;
		return INT_MIN;
	}
	buf[r] = 0;
	res = sscanf(buf,
	 "temperatures: %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
	 &t[0], &t[1], &t[2], &t[3], &t[4], &t[5], &t[6], &t[7], &t[8], &t[9],
	 &t[10], &t[11], &t[12], &t[13], &t[14], &t[15]);
	if (unlikely(res < 2)) {
		message(LOG_ERR, MSG_ERR_T_GET);
		free(t);
		errcnt++;
		return INT_MIN;
	}
	for (i=0; i < 16; i++)
		if (t[i] > retval) retval = t[i];
	free(t);
	return retval;
}

/***********************************************************
 * Set fan speed (IBM interface).
 ***********************************************************/
void setfan_ibm() {
	int ibm_fan;
	char *buf = malloc(9 * sizeof(char));

	if (unlikely((ibm_fan = open(IBM_FAN, O_RDWR, O_TRUNC)) < 0)) {
		showerr(IBM_FAN);
		errcnt++;
	}
	else {
		snprintf(buf, 9, "level %d\n", cur_lvl);
		if (unlikely(write(ibm_fan, buf, 8) != 8)) {
			showerr(IBM_FAN);
			message(LOG_ERR, MSG_ERR_FANCTRL);
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
		showerr(IBM_FAN);
		errcnt++;
		message(LOG_ERR, MSG_ERR_FANFILE_IBM);
		return -1;
	}
	while (getline(&line, &count, ibm_fan) != -1)
		if (!strncmp("commands:", line, 9)) module_valid = 1;
	if (!module_valid) {
		message(LOG_ERR, MSG_ERR_MODOPTS);
		errcnt++;
		return -1;
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
		showerr(IBM_FAN);
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
		showerr(IBM_FAN);
		errcnt++;
	}
	else {
		if (write(ibm_fan, "level disengaged", 16) < 16) {
			showerr(IBM_FAN);
			errcnt++;
		}
		close(ibm_fan);
	}
	if (nanosleep(depulse, NULL)) {
		showerr("nanosleep()");
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
	while(config->sensors[idx] != NULL) {
		if (unlikely((fd = open(config->sensors[idx], O_RDONLY)) == -1
				|| (num = read(fd, &buf, 6)) == -1
				|| close(fd) < 0)) {
			showerr(config->sensors[idx]);
			errcnt++;
			return INT_MIN;
		}
		buf[num] = 0;
		tmp = strtol(buf, &endptr, 10);
		if (tmp > rv) rv = tmp;
		idx++;
	}
	rv = rv/1000;
	if (unlikely(rv < 1)) {
		message(LOG_ERR, MSG_ERR_T_GET);
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
		showerr(config->fan);
		errcnt++;
	}
	else {
		r = snprintf(buf, 5, "%d\n", cur_lvl);
		ret = (int)r + write(fan, buf, 5);
		close(fan);
		if (unlikely(ret < 2)) {
			message(LOG_ERR, MSG_ERR_FANCTRL);
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
	oldpwm = NULL;
	size_t s;
	ssize_t r = 0;

	strcpy(fan_enable, config->fan);
	strcat(fan_enable, "_enable");

	if ((fan = fopen(fan_enable, "r")) == NULL) {
		showerr(fan_enable);
		errcnt++;
		message(LOG_ERR, MSG_ERR_FANFILE_SYSFS(fan_enable));
		free(fan_enable);
		return -1;
	}
	if ((r = getline(&oldpwm, &s, fan)) < 2) showerr(fan_enable);
	fclose(fan);
	free(fan_enable);
	if (r < 2) {
		errcnt++;
		message(LOG_ERR, MSG_ERR_FAN_INIT);
		return -1;
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
		showerr(fan_enable);
		errcnt++;
		message(LOG_ERR, MSG_ERR_FANFILE_SYSFS(fan_enable));
		free(fan_enable);
		return -1;
	}
	if ((r = write(fd, "1\n", 2)) < 2) showerr(fan_enable);
	close(fd);
	free(fan_enable);
	if (r < 2) {
		errcnt++;
		message(LOG_ERR, MSG_ERR_FAN_INIT);
		return -1;
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
			showerr(config->fan);
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

