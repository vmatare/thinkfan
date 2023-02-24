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
#include "sensors.h"
#include "fans.h"
#include "temperature_state.h"


namespace thinkfan {

bool chk_sanity(true);
bool quiet(false);
bool daemonize(true);
seconds sleeptime(5);
seconds tmp_sleeptime = sleeptime;
float bias_level(0);
float depulse = 0;
TemperatureState temp_state(0);
std::atomic<unsigned char> tolerate_errors(0);

std::condition_variable sleep_cond;
std::mutex sleep_mutex;

#ifdef USE_YAML
vector<string> config_files { DEFAULT_YAML_CONFIG, DEFAULT_CONFIG };
#else
vector<std::string> config_files { DEFAULT_CONFIG };
#endif

std::atomic<int> interrupted(0);

#ifdef USE_ATASMART
/** Do Not Disturb disk, i.e. don't get temperature from a sleeping disk */
bool dnd_disk = false;
#endif /* USE_ATASMART */


#if defined(PID_FILE)

PidFileHolder *PidFileHolder::instance_ = nullptr;

PidFileHolder::PidFileHolder(::pid_t pid)
: pid_file_(PID_FILE, std::ios_base::in)
{
	if (!pid_file_.fail())
		error<SystemError>(MSG_RUNNING);
	if (instance_)
		throw Bug("Attempt to initialize PID file twice");
	pid_file_.close();
	pid_file_.open(PID_FILE, std::ios_base::out | std::ios_base::trunc);
	if (!(pid_file_ << pid << std::flush))
		error<IOerror>("Writing to " PID_FILE ": ", errno);
	instance_ = this;
}

PidFileHolder::~PidFileHolder()
{ remove_file(); }

bool PidFileHolder::file_exists()
{ return !ifstream(PID_FILE).fail(); }


void PidFileHolder::remove_file()
{
	if (pid_file_.is_open()) {
		pid_file_.close();
		if (::unlink(PID_FILE) == -1)
			log(TF_ERR) << "Deleting " PID_FILE ": " << errno << "." << flush;
	}
}


void PidFileHolder::cleanup()
{
	if (instance_)
		instance_->remove_file();
}

#else

void PidFileHolder::cleanup()
{}

#endif // defined(PID_FILE)


void sleep(thinkfan::seconds duration) {
	auto until = std::chrono::steady_clock::now() + duration;

	std::unique_lock<std::mutex> sleep_locked(sleep_mutex);
	sleep_cond.wait_until(sleep_locked, until, [] () {
		return interrupted != 0;
	} );
}


void sig_handler(int signum) {
	switch(signum) {
	case SIGHUP:
	case SIGINT:
	case SIGTERM:
		interrupted = signum;
		sleep_cond.notify_all();
		break;
	case SIGUSR1:
		log(TF_NFY) << temp_state << flush;
		break;
#ifndef DISABLE_BUGGER
	case SIGSEGV:
		// Let's hope memory isn't too fucked up to get through with this ;)
		throw Bug("Segmentation fault.");
#endif
	case SIGUSR2:
		interrupted = signum;
		sleep_cond.notify_all();
		log(TF_NFY) << "Received SIGUSR2: Re-initializing fan control." << flush;
		break;
	case SIGPWR:
		tolerate_errors = 4;
		log(TF_NFY) << "Going to sleep: Will allow sensor read errors for the next "
			<< std::to_string(tolerate_errors) << " loops." << flush;
	}
}


void run(const Config &config)
{
	tmp_sleeptime = sleeptime;

	for (const unique_ptr<SensorDriver> &sensor : config.sensors())
		sensor->read_temps();

	// Set initial fan level
	for (auto &fan_config : config.fan_configs())
		fan_config->init_fanspeed(temp_state);
	log(TF_NFY) << temp_state << " -> " << config.fan_configs() << flush;

	bool did_something = false;
	while (likely(!interrupted)) {
		sleep(tmp_sleeptime);

		if (unlikely(interrupted))
			break;

		for (const unique_ptr<SensorDriver> &sensor : config.sensors())
			sensor->read_temps();

		if (unlikely(tolerate_errors) > 0)
			tolerate_errors--;

		for (auto &fan_config : config.fan_configs())
			did_something |= fan_config->set_fanspeed(temp_state);

		if (unlikely(did_something))
			log(TF_NFY) << temp_state << " -> " << config.fan_configs() << flush;

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
			log(TF_NFY) << MSG_TITLE << flush << MSG_USAGE << flush;
			return 1;
#ifdef USE_ATASMART
		case 'd':
			dnd_disk = true;
			break;
#endif
		case 'c':
			config_files = vector<string>({optarg});
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
		log(TF_NFY) << MSG_DEPULSE(depulse, sleeptime.count()) << flush;

	return 0;
}


void noop()
{}



} // namespace thinkfan


int main(int argc, char **argv) {
	using namespace thinkfan;

	struct sigaction handler;
#if defined(PID_FILE)
	unique_ptr<PidFileHolder> pid_file;
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
	 || sigaction(SIGPWR, &handler, nullptr)
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
			LogLevel old_lvl = Logger::instance().log_lvl();
			{
				// Test the config before forking
				unique_ptr<const Config> test_cfg(Config::read_config(config_files));

				Logger::instance().log_lvl() = TF_ERR;

				temp_state = test_cfg->init_sensors();
				test_cfg->init_temperature_refs(temp_state);
				test_cfg->init_fans();

				for (auto &sensor : test_cfg->sensors())
					sensor->read_temps();

				// Own scope so the config gets destroyed before forking
			}
			Logger::instance().log_lvl() = old_lvl;


			pid_t child_pid = ::fork();
			if (child_pid < 0) {
				string msg(strerror(errno));
				error<SystemError>("Can't fork(): " + msg);
			}
			else if (child_pid > 0) {
				log(TF_NFY) << "Daemon PID: " << child_pid << flush;
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
		unique_ptr<const Config> config(Config::read_config(config_files));

		do {
			temp_state = config->init_sensors();
			config->init_temperature_refs(temp_state);

			config->init_fans();
			run(*config);

			if (interrupted == SIGHUP) {
				log(TF_NFY) << MSG_RELOAD_CONF << flush;
				try {
					unique_ptr<const Config> config_new(Config::read_config(config_files));
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

		log(TF_NFY) << MSG_TERM << flush;
#if not defined(DISABLE_EXCEPTION_CATCHING)
	}
	catch (InvocationError &e) {
		log(TF_ERR) << e.what() << flush;
		log(TF_NFY) << MSG_USAGE << flush;
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




