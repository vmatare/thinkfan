/********************************************************************
 * thinkfan.cpp: Main program.
 * (C) 2015, Victor Mataré
 *
 * this file is part of thinkfan.
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
#include <cmath>

#include <unistd.h>

#include "thinkfan.h"
#include "config.h"
#include "message.h"
#include "error.h"


namespace thinkfan {

bool chk_sanity(true);
bool quiet(false);
bool daemonize(true);
seconds sleeptime(5);
seconds tmp_sleeptime = sleeptime;
float bias_level(1.5);
float depulse = 0;
TemperatureState temp_state(0);

std::condition_variable sleep_cond;
std::mutex sleep_mutex;


#ifdef USE_YAML
std::vector<string> config_files { DEFAULT_YAML_CONFIG, DEFAULT_CONFIG };
#else
std::vector<std::string> config_files { DEFAULT_CONFIG };
#endif

std::atomic<int> interrupted(0);

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
		sleep_cond.notify_all();
		break;
	case SIGUSR1:
		log(TF_INF) << temp_state << flush;
		break;
#ifndef DISABLE_BUGGER
	case SIGSEGV:
		// Let's hope memory isn't too fucked up to get through with this ;)
		throw Bug("Segmentation fault.");
#endif
	case SIGUSR2:
		interrupted = signum;
		sleep_cond.notify_all();
		log(TF_INF) << "Received SIGUSR2: Re-initializing fan control." << flush;
		break;
	}
}



static inline void sensor_lost(const SensorDriver &s, const ExpectedError &e) {
	if (!s.optional())
		error<SensorLost>(e);
	else
		log(TF_INF) << SensorLost(e).what();
	temp_state.add_temp(-128);
}


static inline void read_temps_safe(const std::vector<std::unique_ptr<SensorDriver>> &sensors)
{
	temp_state.restart();
	for (const std::unique_ptr<SensorDriver> &sensor : sensors) {
		try {
			sensor->read_temps();
		} catch (SystemError &e) {
			sensor_lost(*sensor, e);
		} catch (IOerror &e) {
			sensor_lost(*sensor, e);
		} catch (std::ios_base::failure &e) {
			sensor_lost(*sensor, IOerror(e.what(), THINKFAN_IO_ERROR_CODE(e)));
		}
	}
}


void run(const Config &config)
{
	tmp_sleeptime = sleeptime;

	read_temps_safe(config.sensors());
	temp_state.init();

	// Set initial fan level
	for (auto &fan_config : config.fan_configs())
		fan_config->init_fanspeed(temp_state);
	log(TF_NFY) << temp_state << " -> " << config.fan_configs() << flush;

	bool did_something = false;
	while (likely(!interrupted)) {
		auto until = std::chrono::steady_clock::now() + tmp_sleeptime;

		std::unique_lock<std::mutex> sleep_locked(sleep_mutex);
		sleep_cond.wait_until(sleep_locked, until, [] () {
			return interrupted != 0;
		} );
		if (unlikely(interrupted))
			break;

		read_temps_safe(config.sensors());

		if (unlikely(!temp_state.complete()))
			throw SystemError(MSG_SENSOR_LOST);

		for (auto &fan_config : config.fan_configs())
			did_something |= fan_config->set_fanspeed(temp_state);

		if (unlikely(did_something))
			log(TF_INF) << temp_state << " -> " << config.fan_configs() << flush;

		did_something = false;
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
	int opt;
	while ((opt = getopt(argc, argv, optstring)) != -1) {
		switch(opt) {
		case 'h':
			log(TF_INF) << MSG_TITLE << flush << MSG_USAGE << flush;
			return 1;
#ifdef USE_ATASMART
		case 'd':
			dnd_disk = true;
			break;
#endif
		case 'c':
			config_files = std::vector<string>({optarg});
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
					s = int(std::stoul(arg, &invalid));
					if (invalid < arg.length())
						throw InvocationError(MSG_OPT_S_INVAL(optarg));
					if (s > 15)
						throw InvocationError(MSG_OPT_S_15(s));
					else if (s < 0)
						throw InvocationError("Negative sleep time? Seriously?");
					else if (s < 1)
						throw InvocationError(MSG_OPT_S_1(s));
					sleeptime = seconds(static_cast<unsigned int>(s));
				} catch (std::invalid_argument &) {
					throw InvocationError(MSG_OPT_S_INVAL(optarg));
				} catch (std::out_of_range &) {
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
				} catch (std::invalid_argument &) {
					throw InvocationError(MSG_OPT_B_INVAL(optarg));
				} catch (std::out_of_range &) {
					throw InvocationError(MSG_OPT_B_INVAL(optarg));
				}
			}
			else throw InvocationError(MSG_OPT_B_NOARG);
			break;
		case 'p':
			if (optarg) {
				size_t invalid;
				string arg(optarg);
				depulse = std::stof(arg, &invalid);
				if (invalid < arg.length() || depulse > 10 || depulse < 0)
					error<InvocationError>(MSG_OPT_P(optarg));
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


#if defined(PID_FILE)
PidFileHolder::PidFileHolder(::pid_t pid)
: pid_file_(PID_FILE, std::ios_base::in)
{
	if (!pid_file_.fail())
		error<SystemError>(MSG_RUNNING);
	pid_file_.close();
	pid_file_.open(PID_FILE, std::ios_base::out | std::ios_base::trunc);
	if (!(pid_file_ << pid << std::flush))
		error<IOerror>("Writing to " PID_FILE ": ", errno);
}


PidFileHolder::~PidFileHolder()
{
	pid_file_.close();
	if (::unlink(PID_FILE) == -1)
		log(TF_ERR) << "Deleting " PID_FILE ": " << errno << "." << flush;
}


bool PidFileHolder::file_exists()
{ return !ifstream(PID_FILE).fail(); }
#endif // defined(PID_FILE)


TemperatureState::TemperatureState(unsigned int num_temps)
: temps_(num_temps, 0),
  biases_(num_temps, 0),
  biased_temps_(num_temps, 0),
  temp_(temps_.begin()),
  bias_(biases_.begin()),
  biased_temp_(biased_temps_.begin()),
  tmax(biased_temps_.begin())
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
		float tmp_bias = float(diff) * bias_level;

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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal" // bias is set to 0 explicitly
			if (unlikely(*bias_ != 0)) {
#pragma GCC diagnostic pop
				if (std::abs(*bias_) < 0.5f)
					*bias_ = 0;
				else
					*bias_ -= 1 + *bias_/5 ;
			}
		}
	}

	*biased_temp_ = *temp_ + int(*bias_);

	if (*biased_temp_ > *tmax)
		tmax = biased_temp_;

	++temp_;
	++bias_;
	++biased_temp_;
}


bool TemperatureState::complete() const
{ return temp_ == temps_.end(); }


const std::vector<int> & TemperatureState::biased_temps() const
{ return biased_temps_; }


const std::vector<int> & TemperatureState::temps() const
{ return temps_; }


const std::vector<float> & TemperatureState::biases() const
{ return biases_; }


void TemperatureState::init()
{
	biases_.clear();
	biases_.resize(temps_.size(), 0);
	biased_temps_ = temps_;
}

} // namespace thinkfan


int main(int argc, char **argv) {
	using namespace thinkfan;

	struct sigaction handler;
#if defined(PID_FILE)
	std::unique_ptr<PidFileHolder> pid_file;
#endif

	if (!isatty(fileno(stdout))) {
		Logger::instance().enable_syslog();
	}

#if not defined(DISABLE_BUGGER)
	std::set_terminate(handle_uncaught);
#endif

	memset(&handler, 0, sizeof(handler));
	handler.sa_handler = sig_handler;

	if (sigaction(SIGHUP, &handler, nullptr)
	 || sigaction(SIGINT, &handler, nullptr)
	 || sigaction(SIGTERM, &handler, nullptr)
	 || sigaction(SIGUSR1, &handler, nullptr)
#if not defined(DISABLE_BUGGER)
	 || sigaction(SIGSEGV, &handler, nullptr)
#endif
	 || sigaction(SIGUSR2, &handler, nullptr)) {
		string msg = strerror(errno);
		log(TF_ERR) << "sigaction: " << msg;
		return 1;
	}

#if not defined(DISABLE_EXCEPTION_CATCHING)
	try {
#endif // DISABLE_EXCEPTION_CATCHING
		switch (set_options(argc, argv)) {
		case 1:
			return 0;
		case 0:
			break;
		default:
			return 3;
		}

#if defined(PID_FILE)
		if (PidFileHolder::file_exists())
			error<SystemError>(MSG_RUNNING);
#endif

		if (daemonize) {
			{
				// Test the config before forking
				LogLevel old_lvl = Logger::instance().log_lvl();
				Logger::instance().log_lvl() = TF_ERR;
				std::unique_ptr<const Config> test_cfg(Config::read_config(config_files));
				temp_state = TemperatureState(test_cfg->num_temps());
				temp_state.init();
				test_cfg->init_fans();
				for (auto &sensor : test_cfg->sensors())
					sensor->read_temps();
				Logger::instance().log_lvl() = old_lvl;
				// Own scope so the config gets destroyed before forking
			}

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
#if defined(PID_FILE)
				// Own PID file only in the child...
				pid_file.reset(new PidFileHolder(::getpid()));
#endif
			}
		}
#if defined(PID_FILE)
		else {
			// ... or when we're not forking at all
			pid_file.reset(new PidFileHolder(::getpid()));
		}
#endif

		// Load the config for real after forking & enabling syslog
		std::unique_ptr<const Config> config(Config::read_config(config_files));

		temp_state = TemperatureState(config->num_temps());

		do {
			config->init_fans();
			run(*config);

			if (interrupted == SIGHUP) {
				log(TF_INF) << MSG_RELOAD_CONF << flush;
				try {
					std::unique_ptr<const Config> config_new(Config::read_config(config_files));
					config.swap(config_new);
				} catch(ExpectedError &) {
					log(TF_ERR) << MSG_CONF_RELOAD_ERR << flush;
				} catch(std::exception &e) {
					log(TF_ERR) << "read_config: " << e.what() << flush;
					log(TF_ERR) << MSG_CONF_RELOAD_ERR << flush;
				}
				interrupted = 0;
			}
			else if (interrupted == SIGUSR2) {
				config->init_fans();
				interrupted = 0;
			}
		} while (!interrupted);

		log(TF_INF) << MSG_TERM << flush;
#if not defined(DISABLE_EXCEPTION_CATCHING)
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
#endif // DISABLE_EXCEPTION_CATCHING

	return 0;
}




