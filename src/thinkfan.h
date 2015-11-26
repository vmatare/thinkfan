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

#define VERSION "0.99.0"
#define PID_FILE "/var/run/thinkfan.pid"
#define CONFIG_DEFAULT "/etc/thinkfan.conf"

#ifndef DEFAULT_FAN
#define DEFAULT_FAN "/proc/acpi/ibm/fan"
#endif

#ifndef DEFAULT_SENSOR
#define DEFAULT_SENSOR "/proc/acpi/ibm/thermal"
#endif


// Stolen from the gurus
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

namespace thinkfan {

typedef std::string string;
typedef std::ifstream ifstream;
typedef std::ofstream ofstream;
typedef std::fstream fstream;
typedef std::chrono::duration<unsigned int> seconds;
typedef std::chrono::duration<float> secondsf;

struct TemperatureState {
	std::vector<int> temps;
	int *temp_idx;
	int *b_tmax;
	int last_tmax;
	int tmax;
};

extern bool chk_sanity;
extern bool resume_is_safe;
extern bool quiet;
#ifdef USE_ATASMART
extern bool dnd_disk;
#endif /* USE_ATASMART */
extern seconds sleeptime;
extern float bias_level;
extern volatile int interrupted;
extern struct TemperatureState temp_state;


}
#endif /* THINKFAN_H_ */
