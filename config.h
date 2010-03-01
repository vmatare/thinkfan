/********************************************************************
 * config.h: Declarations for config.c
 *
 * This work is licensed under a Creative Commons Attribution-Share Alike 3.0
 * United States License. See http://creativecommons.org/licenses/by-sa/3.0/us/
 * for details.
 *
 * This file is part of thinkfan. See thinkfan.c for further info.
 * ******************************************************************/

#ifndef CONFIG_H
#define CONFIG_H

#include "globaldefs.h"

struct tf_config *readconfig(char *fname);
int add_sensor(struct tf_config *cfg, struct sensor *sensor);
int add_limit(struct tf_config *cfg, struct limit *limit);
void free_config(struct tf_config *cfg);

#endif
