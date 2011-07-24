/********************************************************************
 * config.h: Declarations for config.c
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

#ifndef CONFIG_H
#define CONFIG_H

#include "globaldefs.h"

struct tf_config *readconfig(char *fname);
int add_sensor(struct tf_config *cfg, struct sensor *sensor);
int add_limit(struct tf_config *cfg, struct limit *limit);
void free_config(struct tf_config *cfg);

#endif
