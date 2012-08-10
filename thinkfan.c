/*************************************************************************
 * thinkfan version 0.8 -- copyleft 07-2011, Victor Mataré
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
 * Although it's generally considered bad style, I do make heavy use of
 * global variables, hoping to minimize memory access in the inner loop.
 * If you know better, please drop me a line. I haven't yet bothered looking
 * at the difference it makes in machine language.
 *************************************************************************/
#include "globaldefs.h"
#include <string.h>
#include <syslog.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include "thinkfan.h"
#include "message.h"
#include "config.h" // The system interface is part of the config now...

volatile int interrupted;
unsigned int sleeptime, tmp_sleeptime;

#define set_fan \
	if (!quiet && nodaemon) \
	report(LOG_DEBUG, LOG_DEBUG, MSG_DBG_T_STAT); \
	config->setfan();

int run();

/* Return TRUE if *at least one* upper limit has been reached */
int complex_lvl_up() {
	int i;

	for (i=0; i < num_temps; i++)
		if (temps[i] >= config->limits[lvl_idx].high[i]) return TRUE;
	return FALSE;
}

/* Return TRUE if *all* lower limits have been reached */
int complex_lvl_down() {
	int i;

	for (i=0; i < num_temps && temps[i] <= config->limits[lvl_idx].low[i]; i++);
	if (i >= config->limit_len) return TRUE;
	return FALSE;
}

int simple_lvl_up() {
	if (unlikely(*b_tmax >= config->limits[lvl_idx].high[0])) return TRUE;
	return FALSE;
}

int simple_lvl_down() {
	if (unlikely(*b_tmax <= config->limits[lvl_idx].low[0])) return TRUE;
	return FALSE;
}


/***********************************************************
 * This is the main routine which periodically checks
 * temperatures and adjusts the fan according to config.
 ***********************************************************/
int fancontrol() {
	int bias=0, diff=0, i;
	int wt = watchdog_timeout;

	tmp_sleeptime = sleeptime;

	prefix = "\n"; // makes the output more readable
	config->init_fan();
	if (errcnt) return errcnt;

	prefix = "\n"; // It is set to "" by the output macros

	// Set initial fan level...
	lvl_idx = config->num_limits - 1;

	for (i=0; i < num_temps; i++)
		if (temps[i] > tmax) tmax = temps[i];

	/**********************************************
	 * Main loop. This is the actual fan control.
	 **********************************************/
	while(likely(!interrupted && !errcnt)) {

		last_tmax = tmax;
		// depending on the command line, this might also call depulse()
		config->get_temps();

		// If temperature increased by more than 2 °C since the
		// last cycle, we try to react quickly.
		diff = tmax - last_tmax;
		if (unlikely(diff >= 2)) {
			bias = (tmp_sleeptime * (diff-1)) * bias_level;
			if (tmp_sleeptime > 2) tmp_sleeptime = 2;
		}
		else if (unlikely(tmp_sleeptime < sleeptime)) tmp_sleeptime++;
		*b_tmax = tmax + bias;

		// determine appropriate fan level and activate it
		if (unlikely(lvl_idx < config->num_limits - 1 && config->lvl_up())) {
			while (likely(lvl_idx < config->num_limits - 1 && config->lvl_up())) lvl_idx++;
			set_fan;
		}
		else if (unlikely(lvl_idx > 0 && config->lvl_down())) {
			while (likely(lvl_idx > 0 && config->lvl_down())) lvl_idx--;
			set_fan;
			tmp_sleeptime = sleeptime;
		}

		sleep(tmp_sleeptime); // state-dependant sleeptime

		// Write current fan level to IBM_FAN one cycle before the watchdog
		// timeout ends, to let it know we're alive.
		if (unlikely((wt -= tmp_sleeptime) <= sleeptime)) {
			config->setfan();
			wt = watchdog_timeout;
		}

		// slowly reduce the bias
		if (unlikely(bias != 0)) {
			bias -= (bias_level + 0.1f) * 4;
			if (unlikely(bias < 0)) bias = 0;
		}
	}
	return errcnt;
}



void sigHandler(int signum) {
	switch(signum) {
	case SIGHUP:
		interrupted = signum;
		break;
	case SIGINT:
	case SIGTERM:
		interrupted = signum;
		break;
	}
}


/***************************************************************
 * Main function:
 * Scan for arguments, set options and initialize signal handler
 ***************************************************************/
int main(int argc, char **argv) {
	int opt, ret;
	char *invalid = "";
	struct sigaction handler;

	rbuf = NULL;
	depulse_tmp = 0;
	sleeptime = 5;
	quiet = 0;
	chk_sanity = 1;
	bias_level = 0.5f;
	ret = 0;
	config_file = "/etc/thinkfan.conf";
	nodaemon = 0;
	errcnt = 0;
	resume_is_safe = 0;
	prefix = "\n";
	oldpwm = NULL;
	config = NULL;
	lvl_idx = 0;
	last_tmax = 0;
	tmax = 0;
	temps = NULL;

	openlog("thinkfan", LOG_CONS, LOG_USER);
	syslog(LOG_INFO, "thinkfan " VERSION " starting...");

	interrupted = 0;
	memset(&handler, 0, sizeof(handler));
	handler.sa_handler = sigHandler;
	if (sigaction(SIGHUP, &handler, NULL) \
	 || sigaction(SIGINT, &handler, NULL) \
	 || sigaction(SIGTERM, &handler, NULL)) perror("sigaction");

	while ((opt = getopt(argc, argv, "c:s:b:p::hnqDzd")) != -1) {
		switch(opt) {
		case 'h':
			fprintf(stderr, MSG_USAGE);
			return 0;
			break;
		case 'n':
			nodaemon = 1;
			break;
		case 'c':
			config_file = optarg;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'D':
			chk_sanity = 0;
			break;
		case 'z':
			resume_is_safe = !0;
			break;
		case 's':
			sleeptime = (unsigned int) strtol(optarg, &invalid, 0);
			if (*invalid != 0) {
				report(LOG_ERR, LOG_ERR, MSG_ERR_OPT_S);
				report(LOG_ERR, LOG_INFO, MSG_USAGE);
				ret = 1;
				goto fail;
			}
			break;
		case 'b':
			bias_level = strtof(optarg, &invalid);
			if (*invalid != 0) {
				report(LOG_ERR, LOG_ERR, MSG_ERR_OPT_B);
				report(LOG_ERR, LOG_INFO, MSG_USAGE);
				ret = 1;
				goto fail;
			}
			if (bias_level >= 0 && bias_level <= 20)
				bias_level = 0.1f * bias_level;
			else {
				report(LOG_ERR, LOG_ERR, MSG_ERR_OPT_B);
				report(LOG_ERR, LOG_INFO, MSG_USAGE);
				ret = 1;
				goto fail;
			}
			break;
		case 'p':
			if (optarg) {
				depulse_tmp = strtof(optarg, &invalid);
				if (*invalid != 0 || depulse_tmp > 10 || depulse_tmp < 0) {
					report(LOG_ERR, LOG_ERR, MSG_ERR_OPT_P);
					report(LOG_ERR, LOG_INFO, MSG_USAGE);
					ret = 1;
					goto fail;
				}
			}
			else depulse_tmp = 0.5f;
			depulse = (useconds_t) (1000*1000 * depulse_tmp);
			break;
		default:
			fprintf(stderr, MSG_USAGE);
			return 1;
		}
	}

	if (sleeptime > 15) {
		report(LOG_ERR, LOG_WARNING, MSG_WRN_SLEEPTIME_15);
		report(LOG_ERR, LOG_INFO, MSG_INF_SANITY);
		if (chk_sanity) {
			ret = 1;
			goto fail;
		}
	}
	else if (sleeptime < 1) {
		report(LOG_ERR, LOG_WARNING, MSG_WRN_SLEEPTIME_1);
		report(LOG_ERR, LOG_INFO, MSG_INF_SANITY);
		if (chk_sanity) {
			ret = 1;
			goto fail;
		}
	}
	watchdog_timeout = sleeptime * 6;

	ret = run();

fail:
	free_config(config);
	free(rbuf);
	free(oldpwm);
	free(temps);
	return ret;
}

/********************************************************************
 * Outer loop. Handles signal conditions, runtime cleanup and config
 * reloading
 ********************************************************************/
int run() {
	int ret = 0, childpid;
	struct tf_config *newconfig=NULL;
	FILE *pidfile;
	rbuf = malloc(sizeof(char) * 128);

	prefix = "\n";

	if ((config = readconfig(config_file)) == NULL) {
		report(LOG_ERR, LOG_ERR, MSG_ERR_CONF_NOFILE);
		return ERR_CONF_NOFILE;
	}

	config->init_fan();
	config->get_temps();
	if (errcnt) return errcnt;

	if (chk_sanity && ((pidfile = fopen(PID_FILE, "r")) != NULL)) {
		fclose(pidfile);
		report(LOG_ERR, LOG_WARNING, MSG_ERR_RUNNING);
		if (chk_sanity) return ERR_PIDFILE;
	}

	if (depulse) report(LOG_INFO, LOG_DEBUG, MSG_INF_DEPULSE(sleeptime, depulse_tmp));

	// So we try to detect most errors before forking.

	if (!nodaemon) {
		if ((childpid = fork()) != 0) {
			if (!quiet) fprintf(stderr, "Daemon PID: %d\n", childpid);
			return 0;
		}
		if (childpid < 0) {
			perror("fork()");
			return ERR_FORK;
		}
	}

	if ((pidfile = fopen(PID_FILE, "w+")) == NULL && chk_sanity) {
		report(LOG_ERR, LOG_WARNING, PID_FILE ": %s", strerror(errno));
		return ERR_PIDFILE;
	}
	fprintf(pidfile, "%d\n", getpid());
	fclose(pidfile);

	while (1) {
		interrupted = 0;
		if ((ret = fancontrol())) break;
		else if (interrupted == SIGHUP) {
			report(LOG_DEBUG, LOG_DEBUG, MSG_DBG_CONF_RELOAD);
			if ((newconfig = readconfig(config_file)) != NULL) {
				free_config(config);
				config = newconfig;
			}
			else report(LOG_ERR, LOG_ERR, MSG_ERR_CONF_RELOAD);
		}
		else if (SIGINT <= interrupted && interrupted <= SIGTERM) {
			report(LOG_WARNING, LOG_INFO, "\nCaught deadly signal. ");
			break;
		}
	}

	report(LOG_WARNING, LOG_INFO, MSG_INF_TERM);
	config->uninit_fan();

	unlink(PID_FILE);

	return ret;
}

