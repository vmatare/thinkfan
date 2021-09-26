/********************************************************************
 * thinkfan.h: Global (i.e. process-wide) stuff.
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

#ifndef THINKFAN_H_
#define THINKFAN_H_

#include <vector>
#include <chrono>
#include <fstream>
#include <atomic>
#include <mutex>
#include <condition_variable>

#define DEFAULT_CONFIG "/etc/thinkfan.conf"
#define DEFAULT_YAML_CONFIG "/etc/thinkfan.yaml"

#ifndef DEFAULT_FAN
#define DEFAULT_FAN "/proc/acpi/ibm/fan"
#endif

#ifndef DEFAULT_SENSOR
#define DEFAULT_SENSOR "/proc/acpi/ibm/thermal"
#endif


// Stolen from the gurus
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#if defined(__GNUC__) && __GNUC__ < 5
#define THINKFAN_IO_ERROR_CODE(e) errno
#else
#define THINKFAN_IO_ERROR_CODE(e) e.code().value()
#endif

namespace thinkfan {

typedef std::string string;
typedef std::ifstream ifstream;
typedef std::ofstream ofstream;
typedef std::fstream fstream;
typedef std::chrono::duration<unsigned int> seconds;
typedef std::chrono::duration<double> secondsf;


class TemperatureState {
public:
	TemperatureState(unsigned int num_temps);
	void restart();
	void add_temp(int t);

	const std::vector<int> &biased_temps() const;
	const std::vector<int> &temps() const;
	const std::vector<float> &biases() const;
	bool complete() const;
	void init();
private:
	std::vector<int> temps_;
	std::vector<float> biases_;
	std::vector<int> biased_temps_;
	std::vector<int>::iterator temp_;
	std::vector<float>::iterator bias_;
	std::vector<int>::iterator biased_temp_;
	std::vector<int> noise_counters;
	std::vector<int>::iterator noise_counter;
public:
	std::vector<int>::const_iterator tmax;
};


#if defined(PID_FILE)

class PidFileHolder {
public:
	PidFileHolder(::pid_t pid);
	~PidFileHolder();
	static bool file_exists();
	static void cleanup();
private:
	void remove_file();

	std::fstream pid_file_;
	static PidFileHolder *instance_;
};

#else

class PidFileHolder {
public:
	static void cleanup();
};

#endif // defined(PID_FILE)


// Command line options
extern bool chk_sanity;
extern bool resume_is_safe;
extern bool quiet;
extern bool daemonize;
#ifdef USE_ATASMART
extern bool dnd_disk;
#endif /* USE_ATASMART */
extern seconds sleeptime, tmp_sleeptime;
extern float bias_level;
extern std::atomic<int> interrupted;
extern std::vector<string> config_files;
extern float depulse;
extern std::atomic<unsigned char> tolerate_errors;

extern std::condition_variable sleep_cond;
extern std::mutex sleep_mutex;



}
#endif /* THINKFAN_H_ */
