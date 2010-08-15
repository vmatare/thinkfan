/********************************************************************
 * thinkfan.h: Top-level definitions.
 *
 * This work is licensed under a Creative Commons Attribution-Share Alike 3.0
 * United States License. See http://creativecommons.org/licenses/by-sa/3.0/us/
 * for details.
 *
 * This file is part of thinkfan. See thinkfan.c for further info.
 * ******************************************************************/

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
