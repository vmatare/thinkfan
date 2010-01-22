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
#include "config_parser.h"
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
	int line_count=0, err, max_lvl = 0;
	ssize_t line_len;
	size_t ll;
	struct tf_config *cfg_local;
	char *s_input, *input = NULL;
	void *ret = NULL;
	int delim = '\n';

	prefix = "\n";

	cfg_local = (struct tf_config *) malloc(sizeof(struct tf_config));
	cfg_local = (struct tf_config *) memset(cfg_local, 0, sizeof(struct tf_config));

	if ((cfg_file = fopen(fname, "r")) == NULL) {
		showerr(fname);
		goto end;
	}
	while ((line_len = getdelim(&input, &ll, delim, cfg_file)) >= 0) {
		s_input = input;
		line_count++;
		if ((ret = (void *) parse_sensor(&input))) {
			if ((err = add_sensor(cfg_local, (char *) ret)) == ERR_CONF_MIX)
				message(LOG_ERR, MSG_ERR_CONF_MIX(fname, line_count, input))
			else if (err) goto end;
		}
		else if ((ret = (void *) parse_fan(&input))) {
			if (cfg_local->fan == NULL) cfg_local->fan = ret;
			else {
				message(LOG_WARNING, MSG_ERR_CONF_FAN(fname, line_count, input));
				goto end;
			}
		}
		else if ((ret = (void *) parse_limit(&input))) {
			if ((err = add_limit(cfg_local, (struct thm_tuple *) ret))
					== ERR_CONF_ORDER) {
				message(LOG_ERR, MSG_ERR_CONF_ORDER(fname, line_count, input));
				goto end;
			}
			else if (err == ERR_CONF_LOWHIGH) {
				message(LOG_ERR, MSG_ERR_CONF_LOWHIGH(fname, line_count, input));
				goto end;
			}
			else if (err == WARN_CONF_ORDER)
				message(LOG_WARNING, MSG_ERR_CONF_ORDER(fname, line_count, input))
			else if (err == WARN_CONF_ORDER)
				message(LOG_WARNING, MSG_ERR_CONF_LOWHIGH(fname, line_count, input))
			else if (err) goto end;
			free(ret);

			// remember highest fan level for sanity checking
			if (cfg_local->limits[cfg_local->num_limits - 1].level > max_lvl)
				max_lvl = cfg_local->limits[cfg_local->num_limits - 1].level;
		}
		free(s_input);
		input = NULL;
		s_input = NULL;
	}
	free(input);
	input = NULL;
	if (cfg_local->num_limits <= 0) {
		message(LOG_ERR, MSG_ERR_CONF_PARSE);
		goto end;
	}
	fclose(cfg_file);

	if (cfg_local->fan != NULL && strcmp(cfg_local->fan, IBM_FAN)) {
		// a sysfs PWM fan was specified in the config file
		if (max_lvl <= 7) {
			message(LOG_WARNING, MSG_WRN_FANLVL(max_lvl));
			if (chk_sanity) {
				message(LOG_INFO, MSG_INF_SANITY);
				goto end;
			}
			else message(LOG_INFO, MSG_INF_INSANITY);
		}
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
	 strcmp(cfg_local->sensors[cfg_local->num_sensors - 1], IBM_TEMP)) {
		// one or more sysfs sensors were specified in the config file
		if (depulse) cfg_local->get_temp = depulse_and_get_temp_sysfs;
		else cfg_local->get_temp = get_temp_sysfs;
	}
	else {
		if (depulse) cfg_local->get_temp = depulse_and_get_temp_ibm;
		else cfg_local->get_temp = get_temp_ibm;
	}

	return cfg_local;

end:
	if (cfg_file) fclose(cfg_file);
	free(input);
	free_config(cfg_local);
	return NULL;
}

int add_sensor(struct tf_config *cfg, char* sensor) {
	if (cfg->num_sensors > 1 && !strcmp(sensor, IBM_TEMP)) return ERR_CONF_MIX;
	if ( !( (cfg->sensors = (char **) realloc(cfg->sensors,
			(cfg->num_sensors+2) * sizeof(char *))))) {
		showerr("Allocating memory for config");
		free(sensor);
		return ERR_MALLOC;
	}
	cfg->sensors[cfg->num_sensors] = sensor;
	cfg->sensors[++(cfg->num_sensors)] = NULL;
	return 0;
}

int add_limit(struct tf_config *cfg, struct thm_tuple *limit) {
	int rv = 0;

	// check for correct ordering of fan levels in config
	if ((cfg->num_limits >= 1) && (limit->level <=
			cfg->limits[cfg->num_limits - 1].level)) {
		if (chk_sanity) return ERR_CONF_ORDER;
		else rv = WARN_CONF_ORDER;
	}
	if (limit->high <= limit->low) {
		if (chk_sanity) return ERR_CONF_LOWHIGH;
		else rv = WARN_CONF_LOWHIGH;
	}

	if (!(cfg->limits = (struct thm_tuple *) realloc(
			cfg->limits, sizeof(struct thm_tuple) * (cfg->num_limits + 1)))) {
		showerr("Allocating memory for config");
		return ERR_MALLOC;
	}

	cfg->limits[cfg->num_limits] = *limit;
	cfg->num_limits++;
	return rv;
}

void free_config(struct tf_config *cfg) {
	free(cfg->fan);
	if (cfg->num_sensors > 0) {
		int j;
		for (j=0; j < cfg->num_sensors; j++)
			free(cfg->sensors[j]);
		free(cfg->sensors);
	}
	free(cfg->limits);
	free(cfg);
}


