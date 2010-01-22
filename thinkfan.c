/*************************************************************************
 * thinkfan version 0.6.9 -- copyleft 11-2009, Victor Mataré
 *
 * This work is licensed under a Creative Commons Attribution-Share Alike 3.0
 * United States License.
 * See http://creativecommons.org/licenses/by-sa/3.0/us/ for details.
 *
 * A minimalist, lightweight fan control program for modern Thinkpads.
 *
 * This program is provided to you AS-IS. No warranties whatsoever.
 * If this program steals your car, kills your horse, smokes your dope
 * or pees on your carpet... too bad, you're on your own.
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
#include "thinkfan.h"
#include "message.h"
#include "config.h" // The system interface is part of the config now...


/***********************************************************
 * This is the main routine which periodically checks
 * temperatures and adjusts the fan according to config.
 ***********************************************************/
int fancontrol() {
	int last_temp=0, temp, lvl_idx=0, bias=0, diff=0, b_temp;
	int wt = watchdog_timeout, st = sleeptime;

	prefix = "\n"; // makes the output more readable
	if(config->init_fan()) return errcnt;

	if (!quiet) {
		int i;
		message(LOG_INFO, MSG_INF_CONFIG);
		for (i=0; i < config->num_limits; i++) {
			message(LOG_INFO, MSG_INF_CONF_ITEM(
			 config->limits[i].level, config->limits[i].low,
			 config->limits[i].high));
		}
	}
	if (depulse) message(LOG_INFO, MSG_INF_DEPULSE(sleeptime, depulse_tmp));

	prefix = "\n"; // It is set to "" by the output macros

	// Set initial fan level...
	lvl_idx = config->num_limits - 1;
	b_temp = temp = config->get_temp();
	if (errcnt) return errcnt;
	while ((temp <= config->limits[lvl_idx].low) \
	 && (lvl_idx > 0)) lvl_idx--;
	set_fan;

	/**********************************************
	 * Main loop. This is the actual fan control.
	 **********************************************/
	while(likely(!interrupted && !errcnt)) {
		last_temp = temp; // detect temperature jumps
		sleep(st); // st is the state-dependant sleeptime

		// depending on the command line, this might also call depulse()
		temp = config->get_temp();

		// If temperature increased by more than 2 °C since the
		// last cycle, we try to react quickly.
		diff = temp - last_temp;
		if (unlikely(diff >= 2)) {
			bias = (st * (diff-1)) * bias_level;
			if (st > 2) st = 2;
		}
		else if (unlikely(st < sleeptime)) st++;
		b_temp = temp + bias;

		if (unlikely(b_temp >= config->limits[lvl_idx].high)) {
			while (likely((b_temp >= config->limits[lvl_idx].high) \
			 && (lvl_idx < config->num_limits-1))) lvl_idx++;
			set_fan;
		}
		if (unlikely(b_temp <= config->limits[lvl_idx].low)) {
			while (likely((b_temp <= config->limits[lvl_idx].low) \
			 && (lvl_idx > 0))) lvl_idx--;
			set_fan;
			st = sleeptime;
		}

		// slowly reduce the bias
		if (unlikely(bias != 0)) {
			bias -= (bias_level + 0.1f) * 4;
			if (unlikely(bias < 0)) bias = 0;
		}

		// Write current fan level to IBM_FAN one cycle before the watchdog
		// timeout ends, to let it know we're alive.
		if (unlikely((wt -= st) <= st)) {
			config->setfan();
			wt = watchdog_timeout;
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
	int opt, ret, fork_ret;
	char *invalid = "";
	struct sigaction handler;

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
	depulse = NULL;
	FILE *fd;
	prefix = "\n";

	openlog("thinkfan", LOG_CONS, LOG_USER);
	syslog(LOG_INFO, "thinkfan " VERSION " starting...");

	interrupted = 0;
	memset(&handler, 0, sizeof(handler));
	handler.sa_handler = sigHandler;
	if (sigaction(SIGHUP, &handler, NULL) \
	 || sigaction(SIGINT, &handler, NULL) \
	 || sigaction(SIGTERM, &handler, NULL)) perror("sigaction");

	while ((opt = getopt(argc, argv, "c:s:b:p::hnqDz")) != -1) {
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
		case 's':
			sleeptime = (unsigned int) strtol(optarg, &invalid, 0);
			if (*invalid != 0) {
				message_fg(LOG_ERR, MSG_ERR_OPT_S);
				message_fg(LOG_INFO, MSG_USAGE);
				return 1;
			}
			break;
		case 'q':
			quiet = 1;
			break;
		case 'D':
			chk_sanity = 0;
			break;
		case 'b':
			bias_level = strtof(optarg, &invalid);
			if (*invalid != 0) {
				message_fg(LOG_ERR, MSG_ERR_OPT_B);
				message_fg(LOG_INFO, MSG_USAGE);
				return 1;
			}
			if (bias_level >= 0 && bias_level <= 20)
				bias_level = 0.1f * bias_level;
			else {
				message_fg(LOG_ERR, MSG_ERR_OPT_B);
				message_fg(LOG_INFO, MSG_USAGE);
				return 1;
			}
			break;
		case 'z':
			resume_is_safe = !0;
			break;
		case 'p':
			if (optarg) {
				depulse_tmp = strtof(optarg, &invalid);
				if (*invalid != 0 || depulse_tmp > 10 || depulse_tmp < 0) {
					message_fg(LOG_ERR, MSG_ERR_OPT_P)
					message_fg(LOG_INFO, MSG_USAGE);
					return 1;
				}
			}
			else depulse_tmp = 0.5f;
			depulse = (struct timespec *) malloc(sizeof(struct timespec));
			depulse->tv_sec = (time_t)depulse_tmp;
			depulse->tv_nsec = 1000*1000*1000 * (depulse_tmp - depulse->tv_sec);
			break;
		default:
			fprintf(stderr, MSG_USAGE);
			return 1;
		}
	}

	if (sleeptime > 15) {
		if (chk_sanity) {
			message_fg(LOG_ERR, MSG_WRN_SLEEPTIME_15);
			message_fg(LOG_ERR, MSG_INF_SANITY);
			return 1;
		}
		else {
			message_fg(LOG_WARNING, MSG_WRN_SLEEPTIME_15);
			message_fg(LOG_WARNING, MSG_INF_INSANITY);
		}
	}
	else if (sleeptime < 1) {
		if (chk_sanity) {
			message_fg(LOG_ERR, MSG_WRN_SLEEPTIME_1);
			message_fg(LOG_ERR, MSG_INF_SANITY);
			return 1;
		}
		else {
			message_fg(LOG_WARNING, MSG_WRN_SLEEPTIME_1);
			message_fg(LOG_WARNING, MSG_INF_INSANITY);
		}
	}
	watchdog_timeout = sleeptime * 6;

	if (chk_sanity && ((fd = fopen(PID_FILE, "r")) != NULL)) {
		fclose(fd);
		message_fg(LOG_ERR, MSG_ERR_RUNNING);
		return RV_PIDFILE;
	}

	if (nodaemon) ret = run();
	else {
		if ((fork_ret = fork()) == 0) ret = run();
		else if (!quiet)
			fprintf(stderr, "Daemon PID: %d\n", fork_ret);
		if (fork_ret < 0) {
			perror("fork()");
			ret = 1;
		}
	}
	return ret;
}

/********************************************************************
 * Outer loop. Handles signal conditions, runtime cleanup and config
 * reloading
 ********************************************************************/
int run() {
	int ret = 0;
	struct tf_config *newconfig=NULL;
	FILE *pidfile;

	if ((config = readconfig(config_file)) == NULL) {
		message(LOG_ERR, MSG_ERR_CONF_NOFILE);
		ret = -1;
		goto kapott1;
	}

	prefix = "\n";
	if ((pidfile = fopen(PID_FILE, "w+")) == NULL) {
		showerr(PID_FILE);
		ret = -2;
		goto kapott;
	}
	fprintf(pidfile, "%d\n", getpid());
	fclose(pidfile);

	while (1) {
		interrupted = 0;
		if ((ret = fancontrol())) break;
		else if (interrupted == SIGHUP) {
			message(LOG_DEBUG, MSG_DBG_CONF_RELOAD);
			if ((newconfig = readconfig(config_file)) != NULL) {
				free_config(config);
				config = newconfig;
			}
			else message(LOG_ERR, MSG_ERR_CONF_RELOAD);
		}
		else if (SIGINT <= interrupted && interrupted <= SIGTERM) {
			message(LOG_WARNING, "\nCaught deadly signal. ");
			break;
		}
	}

	message(LOG_WARNING, MSG_INF_TERM);
	config->uninit_fan();
kapott:
	free_config(config);
kapott1:
	free(depulse);
	unlink(PID_FILE);
	return ret;
}

