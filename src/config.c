/********************************************************************
 * config.c: Anything that deals with reading the config file
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
#include "config.h"
#include "parser.h"
#include <syslog.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "message.h"
#include "system.h"
#include "thinkfan.h"

static int add_sensor(struct tf_config *cfg, struct sensor *sensor);
static int add_limit(struct tf_config *cfg, struct limit *limit);


/***********************************************************************
 * readconfig(char *fname) reads the config file and
 * returns a pointer to a struct tf_config. Returns NULL if there's any
 * problem with the config.
 **********************************************************************/
struct tf_config *readconfig(char* fname) {
	int err, i, j, fd, *temps_save;
	struct tf_config *cfg_local, *cfg_save;
	char *s_input = NULL, *input = NULL;
	void *ret = NULL, *map_start;
	struct stat sb;

	line_count = 0;

	prefix = "\n";

	cfg_local = (struct tf_config *) malloc(sizeof(struct tf_config));
	cfg_local = (struct tf_config *) memset(cfg_local, 0, sizeof(struct tf_config));

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		report(LOG_ERR, LOG_ERR, "%s: %s", fname, strerror(errno));
		goto fail;
	}
	if (fstat(fd, &sb) < 0) {
		report(LOG_ERR, LOG_ERR, "%s: %s", fname, strerror(errno));
		goto fail;
	}

	map_start = (char *) mmap(NULL, sb.st_size,
			PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (map_start == MAP_FAILED) {
		report(LOG_ERR, LOG_ERR, "%s: %s", fname, strerror(errno));
		goto fail;
	}
	input = (char *) map_start;

	while (*input != 0) {
		s_input = input;

		// mostly sanity checking and n00b catering...
		if ((ret = (void *) parse_sensor(&input))) {
			*(input-sizeof(char)) = 0;
			if ((err = add_sensor(cfg_local, (struct sensor *) ret)) == ERR_CONF_MIX) {
				report(LOG_ERR, LOG_ERR, MSG_FILE_HDR(fname, s_input));
				report(LOG_ERR, LOG_ERR, MSG_ERR_CONF_MIX);
				goto fail;
			}
			else if (err) goto fail;
		}
		else if ((ret = (void *) parse_fan(&input))) {
			*(input-sizeof(char)) = 0;
			if (cfg_local->fan == NULL) cfg_local->fan = ret;
			else {
				report(LOG_ERR, LOG_WARNING, MSG_FILE_HDR(fname, s_input));
				report(LOG_ERR, LOG_WARNING, MSG_ERR_CONF_FAN);
				if (chk_sanity) goto fail;
			}
		}
		else if ((ret = (void *) parse_level(&input))) {
			*(input-sizeof(char)) = 0;
			if ((err = add_limit(cfg_local, (struct limit *) ret))) {
				if (err & ERR_CONF_LOWHIGH) {
					report(LOG_ERR, LOG_WARNING, MSG_FILE_HDR(fname, s_input));
					report(LOG_ERR, LOG_WARNING, MSG_ERR_CONF_LOWHIGH);
					err ^= ERR_CONF_LOWHIGH;
					if (chk_sanity) goto fail;
				}
				if (err & ERR_CONF_LVLORDER) {
					report(LOG_ERR, LOG_WARNING, MSG_FILE_HDR(fname, s_input));
					report(LOG_ERR, LOG_WARNING, MSG_ERR_CONF_LVLORDER);
					err ^= ERR_CONF_LVLORDER;
					if (chk_sanity) goto fail;
				}
				if (err & ERR_CONF_OVERLAP) {
					report(LOG_ERR, LOG_WARNING, MSG_FILE_HDR(fname, s_input));
					report(LOG_ERR, LOG_WARNING, MSG_ERR_CONF_OVERLAP);
					err ^= ERR_CONF_OVERLAP;
					if (chk_sanity) goto fail;
				}
				if (err & ERR_CONF_LVL0) {
					report(LOG_ERR, LOG_WARNING, MSG_FILE_HDR(fname, s_input));
					report(LOG_ERR, LOG_WARNING, MSG_ERR_CONF_LVL0);
					err ^= ERR_CONF_LVL0;
					if (chk_sanity) goto fail;
				}
				if (err & WRN_CONF_INTMIN_LVL) {
					report(LOG_WARNING, LOG_INFO, MSG_FILE_HDR(fname, s_input));
					report(LOG_WARNING, LOG_INFO, MSG_WRN_LVL_DISENGAGED);
					err ^= WRN_CONF_INTMIN_LVL;
				}
				if (err & ERR_CONF_LVLFORMAT) {
					report(LOG_ERR, LOG_WARNING, MSG_FILE_HDR(fname, s_input));
					report(LOG_ERR, LOG_WARNING, MSG_ERR_CONF_LVLFORMAT);
					err ^= ERR_CONF_LVLFORMAT;
					if (chk_sanity) goto fail;
				}
				if (err & ERR_CONF_LIMITLEN) {
					report(LOG_ERR, LOG_ERR, MSG_FILE_HDR(fname, s_input));
					report(LOG_ERR, LOG_ERR, MSG_ERR_LIMITLEN);
					err ^= ERR_CONF_LIMITLEN;
					goto fail;
				}
			}
			else if (err) goto fail;
		}
		else if ((ret = (void *) parse_comment(&input))) free(ret);
		else if (!parse_blankline(&input)) {
			*(input-sizeof(char)) = 0;
			report(LOG_ERR, LOG_WARNING, MSG_FILE_HDR(fname, s_input));
			report(LOG_ERR, LOG_WARNING, MSG_ERR_CONF_PARSE);
			if (chk_sanity) goto fail;
		}
	}
	if (cfg_local->num_limits <= 0) {
		report(LOG_ERR, LOG_ERR, MSG_ERR_CONF_NOFAN);
		goto fail;
	}

	// configure fan interface
	if (cfg_local->fan != NULL && strcmp(cfg_local->fan, IBM_FAN)) {
		// a sysfs PWM fan was specified in the config file
		if (resume_is_safe) {
			cfg_local->setfan = setfan_sysfs;
		}
		else {
			cfg_local->setfan = setfan_sysfs_safe;
			report(LOG_WARNING, LOG_WARNING, MSG_WRN_SYSFS_SAFE);
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

	// configure sensor interface
	if (cfg_local->num_sensors > 0 &&
	 strcmp(cfg_local->sensors[cfg_local->num_sensors - 1].path, IBM_TEMP)) {
		// one or more sysfs sensors were specified in the config file
		if (depulse) cfg_local->get_temps = depulse_and_get_temps_sysfs;
		else cfg_local->get_temps = get_temps_sysfs;
		num_temps = cfg_local->num_sensors;
		if (errcnt & ERR_T_GET) {
			report(LOG_ERR, LOG_ERR, MSG_ERR_T_GET);
			goto fail;
		}
	}
	else {
		if (depulse) cfg_local->get_temps = depulse_and_get_temps_ibm;
		else cfg_local->get_temps = get_temps_ibm;
		num_temps = count_temps_ibm();
		if (errcnt & ERR_T_GET) {
			report(LOG_ERR, LOG_ERR, MSG_ERR_T_GET);
			goto fail;
		}
		if (cfg_local->num_sensors == 0) {
			report(LOG_WARNING, LOG_NOTICE, MSG_WRN_SENSOR_DEFAULT);
			cfg_local->sensors = malloc(sizeof(struct sensor));
			cfg_local->sensors = memset(cfg_local->sensors, 0,
					sizeof(struct sensor));
			cfg_local->sensors->path = (char *) calloc(
					strlen(IBM_TEMP)+1, sizeof(char));
			strcpy(cfg_local->sensors->path, IBM_TEMP);
			cfg_local->num_sensors++;
		}
	}

	// configure temperature comparison method (new in 0.8)
	if (cfg_local->limits[0].low[1] == INT_MIN) {
		cfg_local->lvl_up = simple_lvl_up;
		cfg_local->lvl_down = simple_lvl_down;
	}
	else {
		if (cfg_local->limit_len < num_temps)
			report(LOG_WARNING, LOG_INFO, MSG_WRN_NUM_TEMPS(
					num_temps, cfg_local->limit_len));
		if (cfg_local->limit_len > num_temps) {
			report(LOG_ERR, LOG_ERR, MSG_ERR_LONG_LIMIT);
			goto fail;
		}
		cfg_local->lvl_up = complex_lvl_up;
		cfg_local->lvl_down = complex_lvl_down;
	}

	/* Bleh. This is awful.
	 * Not sure if cheap function calls are worth this kind of crap code.
	 * See the done: and fail: labels (urgh) */
	temps_save = temps;
	temps = (int *) calloc(num_temps, sizeof(int));
	cfg_save = config;
	config = cfg_local;
	if ((num_temps > 1 && cfg_local->get_temps() != num_temps)
			|| (errcnt & ERR_T_GET)) {
		report(LOG_ERR, LOG_ERR, MSG_ERR_T_GET);
		goto fail;
	}
	config = cfg_save;

	// check for a sane start temperature
	if (cfg_local->limit_len == 1 && chk_sanity
			&& cfg_local->limits[0].high[0] > 48) {
		for (i=0; i < cfg_local->num_sensors; i++)
			for (j=0; j < 16; j++)
				if (cfg_local->sensors[i]
				                       .bias[j] != 0) goto done;
		report(LOG_WARNING, LOG_NOTICE, MSG_WRN_CONF_NOBIAS(
				cfg_local->limits[0].high[0]));
	}

done:
	munmap(map_start, sb.st_size);
	close(fd);
	free(temps_save);
	return cfg_local;

fail:
	free_config(cfg_local);
	free(temps);
	temps = temps_save;
	return NULL;
}



static int find_max(int *l) {
	int i, rv = INT_MIN;
	for (i = 0; l[i] != INT_MIN; i++)
		if (l[i] > rv) rv = l[i];
	return rv;
}



static int add_sensor(struct tf_config *cfg, struct sensor *sensor) {
	if (cfg->num_sensors >= 1 && !strcmp(sensor->path, IBM_TEMP)) {
		if (!strcmp(sensor->path, cfg->sensors[cfg->num_sensors - 1].path))
			return 0;
		else return ERR_CONF_MIX;
	}
	if (!(cfg->sensors = (struct sensor *) realloc(cfg->sensors,
			(cfg->num_sensors+1) * sizeof(struct sensor)))) {
		report(LOG_ERR, LOG_ERR, "Allocating memory for config: %s",
				strerror(errno));
		free(sensor);
		return ERR_MALLOC;
	}
	cfg->sensors[cfg->num_sensors++] = *sensor;
	free(sensor);
	return 0;
}



/* Yep, this is mostly sanity checking... */
static int add_limit(struct tf_config *cfg, struct limit *limit) {
	int rv = 0, nl, nh, i;
	long int tmp;
	char *end, *conv_lvl;

	// Check formatting of level string...
	tmp = strtol(limit->level, &end, 0);
	if (tmp < INT_MIN || tmp > INT_MAX) {
		rv |= ERR_CONF_LVLFORMAT;
	}
	else if (tmp == INT_MIN) {
		// old special value for "level disengaged"
		free(limit->level);
		limit->level = "level disengaged";
		limit->nlevel = (int)tmp;
		rv |= WRN_CONF_INTMIN_LVL;
	}
	else if (*end == 0) {
		// just a number
		conv_lvl = calloc(7 + strlen(limit->level), sizeof(char));
		snprintf(conv_lvl, 7 + strlen(limit->level), "level %d", (int)tmp);
		free(limit->level);
		limit->level = conv_lvl;
		limit->nlevel = (int)tmp;
	}
	else if (sscanf(limit->level, "level %d", (int * )&tmp)) {
		limit->nlevel = (int)tmp;
	}
	else if (!strcmp(limit->level, "level disengaged")
			|| !strcmp(limit->level, "level auto")) {
		limit->nlevel = INT_MIN;
	}
	else {
		// something broken
		rv |= ERR_CONF_LVLFORMAT;
		limit->nlevel = INT_MAX;
	}

	// Check length of limits...
	for (nl = 0; limit->low[nl] != INT_MIN; nl++);
	for (nh = 0; limit->high[nh] != INT_MIN; nh++);
	if (cfg->limit_len <= 0) cfg->limit_len = nl;
	if (nh != cfg->limit_len || nl != cfg->limit_len) {
		rv |= ERR_CONF_LIMITLEN;
		goto bail;
	}

	// Check level ordering and border values...
	if (cfg->num_limits <= 0) {
		if (find_max(limit->low) > 0)
			rv |= ERR_CONF_LVL0;
	}
	else {
		if (limit->nlevel != INT_MAX && limit->nlevel != INT_MIN &&
				cfg->limits[cfg->num_limits-1].nlevel >= limit->nlevel)
			rv |= ERR_CONF_LVLORDER;

		if (cfg->num_limits > 1)
			for (i = 0; i < cfg->limit_len; i++)
				if (cfg->limits[cfg->num_limits-1].high[i] <
						limit->low[i])
					rv |= ERR_CONF_OVERLAP;

		for (i = 0; limit->low[i] != INT_MIN; i++) {
			if (!(limit->low[i] == INT_MAX && limit->high[i] == INT_MAX)
					&& limit->low[i] >= limit->high[i])
				rv |= ERR_CONF_LOWHIGH;
		}
	}

	// ... and FINALLY do what we came for
	if (!(cfg->limits = (struct limit *) realloc(cfg->limits,
			sizeof(struct limit) * (cfg->num_limits + 1)))) {
		report(LOG_ERR, LOG_ERR, "Allocating memory for config: %s",
				strerror(errno));
		rv |= ERR_MALLOC;
		goto bail;
	}
	cfg->limits[cfg->num_limits++] = *limit;

bail:
	free(limit);
	return rv;
}



void free_config(struct tf_config *cfg) {
	int j;
	if (!cfg) return;
	free(cfg->fan);
	if (cfg->num_sensors > 0) {
		for (j=0; j < cfg->num_sensors; j++)
			free(cfg->sensors[j].path);
		free(cfg->sensors);
	}
	for (j=0; j < cfg->num_limits; j++) {
		free(cfg->limits[j].level);
		free(cfg->limits[j].low);
		free(cfg->limits[j].high);
	}
	free(cfg->limits);
	free(cfg);
}

