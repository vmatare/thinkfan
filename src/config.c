/********************************************************************
 * config.c: Anything that deals with reading the config file
 *
 * This work is licensed under a Creative Commons Attribution-Share Alike 3.0
 * United States License. See http://creativecommons.org/licenses/by-sa/3.0/us/
 * for details.
 *
 * This file is part of thinkfan. See thinkfan.c for further info.
 * ******************************************************************/
#include "config.h"
#include "parser.h"
#include <syslog.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include "message.h"
#include "system.h"

/***********************************************************************
 * readconfig(char *fname) reads the config file and
 * returns a pointer to a struct tf_config. Returns NULL if there's any
 * problem with the config.
 * Non-matching lines are skipped.
 **********************************************************************/
struct tf_config *readconfig(char* fname) {
	FILE *cfg_file;
	int line_count=0, err, i, j;
	ssize_t line_len;
	size_t ll;
	struct tf_config *cfg_local;
	char *s_input = NULL, *input = NULL;
	void *ret = NULL;
	int delim = '\n';

	prefix = "\n";

	cfg_local = (struct tf_config *) malloc(sizeof(struct tf_config));
	cfg_local = (struct tf_config *) memset(cfg_local, 0, sizeof(struct tf_config));

	if ((cfg_file = fopen(fname, "r")) == NULL) {
		showerr(fname);
		goto fail;
	}
	while ((line_len = getdelim(&input, &ll, delim, cfg_file)) >= 0) {
		s_input = input;
		line_count++;
		if ((ret = (void *) parse_sensor(&input))) {
			if ((err = add_sensor(cfg_local, (struct sensor *) ret)) == ERR_CONF_MIX) {
				message(LOG_ERR, MSG_FILE_HDR(fname, line_count, s_input));
				message(LOG_ERR, MSG_ERR_CONF_MIX)
				goto fail;
			}
			else if (err) goto fail;
		}
		else if ((ret = (void *) parse_fan(&input))) {
			if (cfg_local->fan == NULL) cfg_local->fan = ret;
			else {
				message(LOG_ERR, MSG_FILE_HDR(fname, line_count, s_input));
				message(LOG_ERR, MSG_ERR_CONF_FAN);
				goto fail;
			}
		}
		else if ((ret = (void *) parse_fan_level(&input))) {
			if ((err = add_limit(cfg_local, (struct limit *) ret))
					==  ERR_CONF_LOWHIGH) {
				message(chk_sanity ? LOG_ERR : LOG_WARNING,
						MSG_FILE_HDR(fname, line_count, s_input));
				message(chk_sanity ? LOG_ERR : LOG_WARNING,	MSG_ERR_CONF_LOWHIGH);
				if (chk_sanity) goto fail;
			}
			else if (err == ERR_CONF_LEVEL) {
				message(chk_sanity ? LOG_ERR : LOG_WARNING,
						MSG_FILE_HDR(fname, line_count, s_input));
				message(chk_sanity ? LOG_ERR : LOG_WARNING,	MSG_ERR_CONF_LEVEL);
				if (chk_sanity) goto fail;
			}
			else if (err == ERR_CONF_OVERLAP) {
				message(chk_sanity ? LOG_ERR : LOG_WARNING,
						MSG_FILE_HDR(fname, line_count, s_input));
				message(chk_sanity ? LOG_ERR : LOG_WARNING, MSG_ERR_CONF_OVERLAP);
				if (chk_sanity) goto fail;
			}
			else if (err) goto fail;
		}
		else if ((ret = (void *) parse_comment(&input))) free(ret);
		else if (!parse_blankline(&input)) {
				message(chk_sanity ? LOG_ERR : LOG_WARNING,
						MSG_FILE_HDR(fname, line_count, s_input));
				message(chk_sanity ? LOG_ERR : LOG_WARNING,	MSG_ERR_CONF_PARSE);
				if (chk_sanity) goto fail;
		}
		free(s_input);
		input = NULL;
		s_input = NULL;
	}
	free(input);
	input = NULL;
	if (cfg_local->num_limits <= 0) {
		message(LOG_ERR, MSG_ERR_CONF_NOFAN);
		goto fail;
	}
	fclose(cfg_file);

	if (cfg_local->fan != NULL && strcmp(cfg_local->fan, IBM_FAN)) {
		// a sysfs PWM fan was specified in the config file
		if (resume_is_safe) {
			cfg_local->setfan = setfan_sysfs;
		}
		else {
			cfg_local->setfan = setfan_sysfs_safe;
			message(LOG_WARNING, MSG_WRN_SYSFS_SAFE);
		}
		cfg_local->init_fan = init_fan_sysfs_once;
		cfg_local->uninit_fan = uninit_fan_sysfs;
	}
	else {
		if (cfg_local->fan == NULL) {
			cfg_local->fan = (char *) calloc(strlen(IBM_FAN)+1, sizeof(char));
			strcpy(cfg_local->fan, IBM_FAN);
		}
		cfg_local->setfan = setfan_ibm;
		cfg_local->init_fan = init_fan_ibm;
		cfg_local->uninit_fan = uninit_fan_ibm;
	}

	if (cfg_local->num_sensors > 0 &&
	 strcmp(cfg_local->sensors[cfg_local->num_sensors - 1].path, IBM_TEMP)) {
		// one or more sysfs sensors were specified in the config file
		if (depulse) cfg_local->get_temp = depulse_and_get_temp_sysfs;
		else cfg_local->get_temp = get_temp_sysfs;
	}
	else {
		if (depulse) cfg_local->get_temp = depulse_and_get_temp_ibm;
		else cfg_local->get_temp = get_temp_ibm;
		if (cfg_local->num_sensors == 0) {
			message(LOG_WARNING, MSG_WRN_SENSOR_DEFAULT);
			cfg_local->sensors = malloc(sizeof(struct sensor));
			cfg_local->sensors = memset(cfg_local->sensors, 0,
					sizeof(struct sensor));
			cfg_local->sensors->path = (char *) calloc(
					strlen(IBM_TEMP)+1, sizeof(char));
			strcpy(cfg_local->sensors->path, IBM_TEMP);
			cfg_local->num_sensors++;
		}
	}

	if (chk_sanity && cfg_local->limits[0].high > 48) {
		for (i=0; i < cfg_local->num_sensors; i++)
			for (j=0; j < 16; j++)
				if (cfg_local->sensors[i].bias[j] != 0) goto done;
		message(LOG_WARNING, MSG_WRN_CONF_NOBIAS(cfg_local->limits[0].high));
	}

done:
	if (!quiet) {
		message(LOG_INFO, MSG_INF_CONFIG);
		for (i=0; i < cfg_local->num_limits; i++) {
			message(LOG_INFO, MSG_INF_CONF_ITEM(
			 cfg_local->limits[i].level, cfg_local->limits[i].low,
			 cfg_local->limits[i].high));
		}
	}

	return cfg_local;

fail:
	if (cfg_file) fclose(cfg_file);
	free(s_input);
	free_config(cfg_local);
	return NULL;
}

int add_sensor(struct tf_config *cfg, struct sensor *sensor) {
	if (cfg->num_sensors >= 1 && !strcmp(sensor->path, IBM_TEMP)) {
		if (!strcmp(sensor->path, cfg->sensors[cfg->num_sensors - 1].path))
			return 0;
		else return ERR_CONF_MIX;
	}
	if (!(cfg->sensors = (struct sensor *) realloc(cfg->sensors,
			(cfg->num_sensors+1) * sizeof(struct sensor)))) {
		showerr("Allocating memory for config");
		free(sensor);
		return ERR_MALLOC;
	}
	cfg->sensors[cfg->num_sensors++] = *sensor;
	free(sensor);
	return 0;
}

int add_limit(struct tf_config *cfg, struct limit *limit) {
	int rv = 0;

	if (cfg->num_limits > 0) {
		if (cfg->limits[cfg->num_limits-1].level >= limit->level)
			rv = ERR_CONF_LEVEL;
		if (cfg->limits[cfg->num_limits-1].high < limit->low)
			rv = ERR_CONF_OVERLAP;
	}
	if (limit->high <= limit->low) rv = ERR_CONF_LOWHIGH;
	if (!(cfg->limits = (struct limit *) realloc(cfg->limits,
			sizeof(struct limit) * (cfg->num_limits + 1)))) {
		showerr("Allocating memory for config");
		return ERR_MALLOC;
	}

	cfg->limits[cfg->num_limits++] = *limit;
	free(limit);
	return rv;
}

void free_config(struct tf_config *cfg) {
	int j;
	free(cfg->fan);
	if (cfg->num_sensors > 0) {
		for (j=0; j < cfg->num_sensors; j++)
			free(cfg->sensors[j].path);
		free(cfg->sensors);
	}
	free(cfg->limits);
	free(cfg);
}

