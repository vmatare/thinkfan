/********************************************************************
 * thinkfan.h: Top-level definitions.
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

#ifndef THINKFAN_H

#define THINKFAN_H
#define _GNU_SOURCE

#include <time.h>

#define set_fan cur_lvl = config->limits[lvl_idx].level; \
		if (!quiet && nodaemon) report(LOG_DEBUG, LOG_DEBUG, MSG_DBG_T_STAT); \
		config->setfan();

volatile int interrupted;
unsigned int sleeptime;

int run();
int fancontrol();

#endif
