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

#include "error.h"

#include <getopt.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/time.h>
#include <sys/resource.h>

#include <csignal>
#include <cstring>
#include <string>
#include <iostream>
#include <memory>
#include <thread>
#include <cmath>

#include <unistd.h>

#include "thinkfan.h"
#include "config.h"
#include "message.h"


namespace thinkfan {

bool chk_sanity(true);
bool quiet(false);
bool daemonize(true);
seconds sleeptime(5);
seconds tmp_sleeptime = sleeptime;
float bias_level(1.5);
int opt;
float depulse = 0;
std::string config_file = CONFIG_DEFAULT;
TemperatureState temp_state(0);
std::unique_ptr<PidFileHolder> pid_file;

volatile int interrupted(0);

#ifdef USE_ATASMART
/** Do Not Disturb disk, i.e. don't get temperature from a sleeping disk */
bool dnd_disk = false;
#endif /* USE_ATASMART */



void sig_handler(int signum) {
	switch(signum) {
	case SIGHUP:
	case SIGINT:
	case SIGTERM:
		interrupted = signum;
		break;
	case SIGUSR1:
		log(TF_INF) << temp_state << flush;
		break;
	case SIGSEGV:
		// Let's hope memory isn't too fucked up to get through with this ;)
		throw Bug("Segmentation fault.");
		break;
	case SIGUSR2:
		interrupted = signum;
		log(TF_INF) << "Received SIGUSR2: Re-initializing fan control." << flush;
		break;
	}
}



void run(const Config &config)
{
	tmp_sleeptime = sleeptime;

	temp_state.restart();
	for (const SensorDriver *sensor : config.sensors())
		sensor->read_temps();
	temp_state.first_run();

	// Set initial fan level
	std::vector<const Level *>::const_iterator cur_lvl = config.levels().begin();
	config.fan()->init();
	while (cur_lvl != config.levels().end() && (*cur_lvl)->up())
		cur_lvl++;
	log(TF_NFY) << temp_state << " -> " <<
			(*cur_lvl)->str() << flush;
	config.fan()->set_speed(*cur_lvl);

	while (likely(!interrupted)) {
		temp_state.restart();

		for (const SensorDriver *sensor : config.sensors())
			sensor->read_temps();
		if (unlikely(!temp_state.complete()))
			throw SystemError(MSG_SENSOR_LOST);

		if (unlikely((*cur_lvl)->up())) {
			while (cur_lvl != config.levels().end() && (*cur_lvl)->up())
				cur_lvl++;
			log(TF_INF) << temp_state << " -> " <<
					(*cur_lvl)->str() << flush;
			config.fan()->set_speed(*cur_lvl);
		}
		else if (unlikely((*cur_lvl)->down())) {
			while (cur_lvl != config.levels().begin() && (*cur_lvl)->down())
				cur_lvl--;
			log(TF_INF) << temp_state << " -> " <<
					(*cur_lvl)->str() << flush;
			config.fan()->set_speed(*cur_lvl);
			tmp_sleeptime = sleeptime;
		}
		else {
			config.fan()->ping_watchdog_and_depulse(*cur_lvl);
#ifdef DEBUG
			log(TF_DBG) << temp_state << flush;
#endif
		}

		std::this_thread::sleep_for(sleeptime);
	}
}


int set_options(int argc, char **argv)
{
	const char *optstring = "c:s:b:p::hqDznv"
#ifdef USE_ATASMART
			"d";
#else
	;
#endif
	opterr = 0;
	while ((opt = getopt(argc, argv, optstring)) != -1) {
		switch(opt) {
		case 'h':
			log(TF_INF) << MSG_TITLE << flush << MSG_USAGE << flush;
			return 1;
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
			--Logger::instance().log_lvl();
			break;
		case 'v':
			++Logger::instance().log_lvl();
			break;
		case 'D':
			chk_sanity = false;
			break;
		case 'z':
			log(TF_WRN) << "Option -z is not needed anymore and therefore deprecated." << flush;
			break;
		case 'n':
			daemonize = false;
			break;
		case 's':
			if (optarg) {
				try {
					size_t invalid;
					int s;
					string arg(optarg);
					s = std::stoul(arg, &invalid);
					if (invalid < arg.length())
						throw InvocationError(MSG_OPT_S_INVAL(optarg));
					if (s > 15)
						throw InvocationError(MSG_OPT_S_15(s));
					else if (s < 0)
						throw InvocationError("Negative sleep time? Seriously?");
					else if (s < 1)
						throw InvocationError(MSG_OPT_S_1(s));
					sleeptime = seconds(static_cast<unsigned int>(s));
				} catch (std::invalid_argument &e) {
					throw InvocationError(MSG_OPT_S_INVAL(optarg));
				} catch (std::out_of_range &e) {
					throw InvocationError(MSG_OPT_S_INVAL(optarg));
				}
			}
			else throw InvocationError(MSG_OPT_S);
			break;
		case 'b':
			if (optarg) {
				try {
					size_t invalid;
					float b;
					string arg(optarg);
					b = std::stof(arg, &invalid);
					if (invalid < arg.length())
						error<InvocationError>(MSG_OPT_B_INVAL(optarg));
					if (b < -10 || b > 30)
						error<InvocationError>(MSG_OPT_B);
					bias_level = b / 10;
				} catch (std::invalid_argument &e) {
					throw InvocationError(MSG_OPT_B_INVAL(optarg));
				} catch (std::out_of_range &e) {
					throw InvocationError(MSG_OPT_B_INVAL(optarg));
				}
			}
			else throw InvocationError(MSG_OPT_B_NOARG);
			break;
		case 'p':
			if (optarg) {
				size_t invalid;
				depulse = std::stof(optarg, &invalid);
				if (invalid != 0 || depulse > 10 )
					error<InvocationError>(MSG_OPT_P(optarg));
				else if (depulse < 0)
					throw InvocationError(MSG_OPT_P(optarg));
			}
			else depulse = 0.5f;
			break;
		default:
			throw InvocationError(string("Unknown option: -") + static_cast<char>(optopt));
		}
	}
	if (depulse > 0)
		log(TF_INF) << MSG_DEPULSE(depulse, sleeptime.count()) << flush;

	return 0;
}


PidFileHolder::PidFileHolder(unsigned int pid)
: pid_file_(PID_FILE, std::ios_base::in)
{
	if (!pid_file_.fail())
		error<SystemError>(MSG_RUNNING);
	pid_file_.close();
	pid_file_.open(PID_FILE, std::ios_base::out | std::ios_base::trunc);
	pid_file_.exceptions(pid_file_.badbit | pid_file_.failbit);
	pid_file_ << pid << std::flush;
}


PidFileHolder::~PidFileHolder()
{
	pid_file_.close();
	if (::unlink(PID_FILE) == -1) {
		string msg = std::strerror(errno);
		throw SystemError("Deleting " PID_FILE ": " + msg);
	}
}


TemperatureState::TemperatureState(unsigned int num_temps)
: temps_(num_temps, 0),
  biases_(num_temps, 0),
  biased_temps_(num_temps, 0),
  temp_(temps_.begin()),
  bias_(biases_.begin()),
  biased_temp_(biased_temps_.begin()),
  tmax(0)
{}


void TemperatureState::restart()
{
	temp_ = temps_.begin();
	bias_ = biases_.begin();
	biased_temp_ = biased_temps_.begin();
	tmax = biased_temps_.begin();
}


void TemperatureState::add_temp(int t)
{
	int diff = t - *temp_;
	*temp_ = t;

	if (unlikely(diff > 2)) {
		// Apply bias_ if temperature changed quickly
		float tmp_bias = (float)diff * bias_level;

		*bias_ = int(tmp_bias);
		if (tmp_sleeptime > seconds(2))
			tmp_sleeptime = seconds(2);
	}
	else {
		if (unlikely(diff < 0)) {
			// Return to normal operation if temperature dropped
			tmp_sleeptime = sleeptime;
			*bias_ = 0;
		}
		else {
			// Slowly return to normal sleeptime
			if (unlikely(tmp_sleeptime < sleeptime)) tmp_sleeptime++;
			// slowly reduce the bias_
			if (unlikely(*bias_ != 0)) {
				if (std::abs(*bias_) < 0.5)
					*bias_ = 0;
				else
					*bias_ -= 1 + *bias_/5 ;
			}
		}
	}

	*biased_temp_ = *temp_ + *bias_;

	if (*biased_temp_ > *tmax)
		tmax = biased_temp_;

	++temp_;
	++bias_;
	++biased_temp_;
}


bool TemperatureState::complete() const
{ return temp_ == temps_.end(); }


const std::vector<int> &TemperatureState::get() const
{ return biased_temps_; }


const std::vector<float> &TemperatureState::biases() const
{ return biases_; }


void TemperatureState::first_run()
{
	biases_.clear();
	biases_.resize(temps_.size(), 0);
	biased_temps_ = temps_;
}

}


int main(int argc, char **argv) {
	using namespace thinkfan;

	struct sigaction handler;

	if (!isatty(fileno(stdout))) {
		Logger::instance().enable_syslog();
	}

	std::set_terminate(handle_uncaught);

	memset(&handler, 0, sizeof(handler));
	handler.sa_handler = sig_handler;

	// Install signal handler only after FanControl object has been created
	// since it is used by the handler.
	if (sigaction(SIGHUP, &handler, NULL)
	 || sigaction(SIGINT, &handler, NULL)
	 || sigaction(SIGTERM, &handler, NULL)
	 || sigaction(SIGUSR1, &handler, NULL)
	 || sigaction(SIGUSR2, &handler, NULL)
	 || sigaction(SIGSEGV, &handler, NULL)) {
		string msg = strerror(errno);
		log(TF_ERR) << "sigaction: " << msg;
		return 1;
	}


	try {
		switch (set_options(argc, argv)) {
		case 1:
			return 0;
			break;
		case 0:
			break;
		default:
			return 3;
		}

		// Load the config temporarily once so we may fail before forking
		LogLevel old_lvl = Logger::instance().set_log_lvl(TF_ERR);
		delete Config::read_config(config_file);
		Logger::instance().set_log_lvl(old_lvl);

		if (daemonize) {
			pid_t child_pid = ::fork();
			if (child_pid < 0) {
				string msg(strerror(errno));
				error<SystemError>("Can't fork(): " + msg);
			}
			else if (child_pid > 0) {
				log(TF_INF) << "Daemon PID: " << child_pid << flush;
				return 0;
			}
			else {
				Logger::instance().enable_syslog();
				// Own PID file only in the child...
				pid_file.reset(new PidFileHolder(::getpid()));
			}
		}
		else {
			// ... or when we're not forking at all
			pid_file.reset(new PidFileHolder(::getpid()));
		}

		// Load the config for real after forking & enabling syslog
		std::unique_ptr<const Config> config(Config::read_config(config_file));
		temp_state = TemperatureState(config->num_temps());

		do {
			run(*config);

			if (interrupted == SIGHUP) {
				log(TF_INF) << MSG_RELOAD_CONF << flush;
				try {
					std::unique_ptr<const Config> config_new(Config::read_config(config_file));
					config.swap(config_new);
				} catch(ExpectedError &e) {
					log(TF_ERR) << MSG_CONF_RELOAD_ERR << flush;
				} catch(std::exception &e) {
					log(TF_ERR) << "read_config: " << e.what() << flush;
					log(TF_ERR) << MSG_CONF_RELOAD_ERR << flush;
				}
				interrupted = 0;
			}
			else if (interrupted == SIGUSR2) {
				config->fan()->init();
				interrupted = 0;
			}
		} while (!interrupted);

		log(TF_INF) << MSG_TERM << flush;
	}
	catch (InvocationError &e) {
		log(TF_ERR) << e.what() << flush;
		log(TF_INF) << MSG_USAGE << flush;
	}
	catch (ExpectedError &e) {
		log(TF_ERR) << e.what() << flush;
		return 1;
	}
	catch (Bug &e) {
		log(TF_ERR) << e.what() << flush <<
				"Backtrace:" << flush <<
				e.backtrace() << flush <<
				MSG_BUG << flush;
		return 2;
	}

	return 0;
}




