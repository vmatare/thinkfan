/********************************************************************
 * thinkfan.cpp: Main program.
 * (C) 2015, Victor Mataré
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

#include <getopt.h>
#include <signal.h>
#include <signal.h>
#include <cstring>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <sys/time.h>
#include <sys/resource.h>

#include <iostream>
#include <memory>
#include <system_error>
#include <thread>

#include "thinkfan.h"
#include "config.h"
#include "message.h"
#include "error.h"


namespace thinkfan {

bool chk_sanity(true);
bool resume_is_safe(false);
bool quiet(false);
std::chrono::duration<unsigned int> sleeptime(5);
float bias_level(5);
volatile int interrupted(0);
#ifdef USE_ATASMART
/** Do Not Disturb disk, i.e. don't get temperature from a sleeping disk */
bool dnd_disk = false;
#endif /* USE_ATASMART */


struct TemperatureState temp_state;


string report_tstat() {
	string rv = "Current temperatures: ";
	for (int temp : temp_state.temps) {
		rv += std::to_string(temp) + ", ";
	}
	return rv.substr(0, rv.length()-2);
}


void sig_handler(int signum) {
	switch(signum) {
	case SIGHUP:
		interrupted = signum;
		break;
	case SIGINT:
	case SIGTERM:
		interrupted = signum;
		break;
	case SIGUSR1:
		log(TF_INF, TF_INF) << report_tstat() << flush;
		break;
	case SIGSEGV:
		struct rlimit core_sz;
		log(TF_ERR, TF_ERR) << "Segmentation fault." << flush;
		if (getrlimit(RLIMIT_CORE, &core_sz) == -1) {
			string msg = strerror(errno);
			log(TF_ERR, TF_ERR) << SystemError("getrlimit(): " + msg) << flush;
		}
		else {
			if (core_sz.rlim_cur <= 0)
				log(TF_ERR, TF_ERR)
				<< "Please enable core dumps with \"ulimit -c unlimited\","
				<< " trigger this error again"
				<< " and attach the core file to a bug report. Thanks."
				<< flush;
			else
				log(TF_ERR, TF_ERR) << "Please file a bug report with the core dump attached "
				<< "at https://github.com/vmatare/thinkfan/issues/new ." << flush
				<< "Thank you." << flush;
		}
		abort();
		break;
	}
}


void run(const Config &config)
{
	std::vector<const Level *>::const_iterator cur_lvl = config.levels().begin();
	temp_state.temps.resize(config.num_temps());

	config.fan()->init();

	float bias = 0;
	seconds tmp_sleeptime = sleeptime;

	temp_state.temp_idx = temp_state.temps.data();
	temp_state.tmax = -128;
	for (const SensorDriver *sensor : config.sensors()) sensor->read_temps();

	while (likely(!interrupted)) {
		temp_state.temp_idx = temp_state.temps.data();
		temp_state.last_tmax = temp_state.tmax;
		temp_state.tmax = -128;

		for (const SensorDriver *sensor : config.sensors())
			sensor->read_temps();

		int diff = temp_state.tmax - temp_state.last_tmax;
		if (unlikely(diff > 2)) {
			// Apply bias if temperature changed quickly
			bias = ((float)diff * bias_level);
			if (tmp_sleeptime > seconds(2)) tmp_sleeptime = seconds(2);
		}
		else {
			if (unlikely(diff < 0)) {
				// Return to normal operation if temperature dropped
				tmp_sleeptime = sleeptime;
				bias = 0;
			}
			// Otherwise slowly return to normal sleeptime
			else if (unlikely(tmp_sleeptime < sleeptime)) tmp_sleeptime++;
		}
		// Apply bias to maximum temperature only
		*temp_state.b_tmax = temp_state.tmax + bias;

		if (unlikely(temp_state.temp_idx - 1 != &temp_state.temps.back()))
			fail(TF_ERR) << SystemError(MSG_SENSOR_LOST) << flush;

		if (unlikely(**cur_lvl <= temp_state)) {
			while (cur_lvl != config.levels().end() && **cur_lvl <= temp_state)
				cur_lvl++;
			log(TF_DBG, TF_DBG) << MSG_T_STAT(tmp_sleeptime.count(), temp_state.tmax,
					temp_state.last_tmax, *temp_state.b_tmax,
					(*cur_lvl)->str()) << flush;
			config.fan()->set_speed((*cur_lvl)->str());
		}
		else if (unlikely(**cur_lvl > temp_state)) {
			while (cur_lvl != config.levels().begin() && **cur_lvl > temp_state)
				cur_lvl--;
			log(TF_DBG, TF_DBG) << MSG_T_STAT(tmp_sleeptime.count(), temp_state.tmax,
					temp_state.last_tmax, *temp_state.b_tmax,
					(*cur_lvl)->str()) << flush;
			config.fan()->set_speed((*cur_lvl)->str());
			tmp_sleeptime = sleeptime;
		}
		else config.fan()->ping_watchdog_and_depulse((*cur_lvl)->str());

		std::this_thread::sleep_for(sleeptime);

		// slowly return bias to 0
		if (unlikely(bias != 0)) {
			if (likely(bias > 0)) {
				if (bias < 0.5) bias = 0;
				else bias -= bias/2 * bias_level;
			}
			else {
				if (bias > -0.5) bias = 0;
				else bias += bias/2 * bias_level;
			}
		}
	}
}

}


int main(int argc, char **argv) {
	using namespace thinkfan;

	struct sigaction handler;
	int opt;
	float depulse = 0;
	std::string config_file = CONFIG_DEFAULT;

	const char *optstring = "c:s:b:p::hqDz"
#ifdef USE_ATASMART
			"d";
#else
	;
#endif

	if (!isatty(fileno(stdout))) Logger::instance().enable_syslog();


	memset(&handler, 0, sizeof(handler));
	handler.sa_handler = sig_handler;

	// Install signal handler only after FanControl object has been created
	// since it is used by the handler.
	if (sigaction(SIGHUP, &handler, NULL) \
	 || sigaction(SIGINT, &handler, NULL) \
	 || sigaction(SIGTERM, &handler, NULL) \
	 || sigaction(SIGUSR1, &handler, NULL) \
	 || sigaction(SIGSEGV, &handler, NULL)) {
		string msg = strerror(errno);
		log(TF_ERR, TF_ERR) << "sigaction: " << msg;
	}

	try {
		while ((opt = getopt(argc, argv, optstring)) != -1) {
			switch(opt) {
			case 'h':
				std::cerr << MSG_USAGE << std::endl;
				return 0;
				break;
#ifdef USE_ATASMART
			case 'd':
				dnd_disk = true;
				break;
#endif
			case 'c':
				config_file = optarg;
				break;
			case 'q':
				quiet = true;
				break;
			case 'D':
				chk_sanity = false;
				break;
			case 'z':
				resume_is_safe = true;
				break;
			case 's':
				if (optarg) {
					try {
						size_t invalid;
						int s;
						string arg(optarg);
						s = std::stoul(arg, &invalid);
						if (invalid < arg.length())
							fail(TF_ERR) << InvocationError(MSG_OPT_S_INVAL(optarg)) << flush;
						if (s > 15)
							fail(TF_WRN) << InvocationError(MSG_OPT_S_15(s)) << flush;
						else if (s < 0)
							fail(TF_ERR) << InvocationError("Negative sleep time? Seriously?") << flush;
						else if (s < 1)
							fail(TF_WRN) << InvocationError(MSG_OPT_S_1(s)) << flush;
						sleeptime = seconds(static_cast<unsigned int>(s));
					} catch (std::invalid_argument &e) {
						fail(TF_ERR) << InvocationError(MSG_OPT_S_INVAL(optarg)) << flush;
					} catch (std::out_of_range &e) {
						fail(TF_ERR) << InvocationError(MSG_OPT_S_INVAL(optarg)) << flush;
					}
				}
				else fail(TF_ERR) << InvocationError(MSG_OPT_S) << flush;
				break;
			case 'b':
				if (optarg) {
					try {
						size_t invalid;
						float b;
						string arg(optarg);
						b = std::stof(arg, &invalid);
						if (invalid < arg.length())
							fail(TF_WRN) << InvocationError(MSG_OPT_B_INVAL(optarg)) << flush;
						if (b < -10 || b > 30) {
							fail(TF_WRN) << InvocationError(MSG_OPT_B) << flush;
						}
						bias_level = b / 10;
					} catch (std::invalid_argument &e) {
						fail(TF_ERR) << InvocationError(MSG_OPT_B_INVAL(optarg)) << flush;
					} catch (std::out_of_range &e) {
						fail(TF_ERR) << InvocationError(MSG_OPT_B_INVAL(optarg)) << flush;
					}
				}
				else fail(TF_ERR) << InvocationError(MSG_OPT_B_NOARG) << flush;
				break;
			case 'p':
				if (optarg) {
					size_t invalid;
					depulse = std::stof(optarg, &invalid);
					if (invalid != 0 || depulse > 10 || depulse < 0) {
						fail(depulse < 0 ? TF_ERR : TF_WRN) << InvocationError(MSG_OPT_P(optarg)) << flush;
					}
				}
				else depulse = 0.5f;
				break;
			default:
				std::cerr << MSG_USAGE << std::endl;
				return 3;
			}
		}
		if (depulse > 0) log(TF_INF, TF_INF) << MSG_DEPULSE(depulse, sleeptime.count()) << flush;

		do {
			const Config *config = Config::read_config(config_file);
			run(*config);
			delete config;
		} while (interrupted == SIGHUP);

		log(TF_INF, TF_INF) << MSG_TERM << flush;
	}
	catch (Error &e) {
		// Our own exceptions are thrown via the logger, thus no output here.
		return 1;
	}
	catch (std::exception &e) {
		string msg(strerror(errno));
		std::cerr << e.what() << ":" << std::endl << msg << std::endl;
		return 2;
	}

	return 0;
}

