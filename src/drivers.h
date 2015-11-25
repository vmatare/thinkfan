/********************************************************************
 * config.h: Config data structures and consistency checking.
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

#ifndef THINKFAN_DRIVERS_H_
#define THINKFAN_DRIVERS_H_

#include <string>

#include "thinkfan.h"

#ifdef USE_ATASMART
#include <atasmart.h>
#endif /* USE_ATASMART */

#ifdef USE_NVML
#include <nvidia/gdk/nvml.h>
#endif /* USE_NVML */

namespace thinkfan {

class Level;

class FanDriver {
protected:
	string path_;
	string initial_state_;
	std::chrono::duration<unsigned int> watchdog_;
	std::chrono::duration<float> depulse_;
	std::chrono::system_clock::time_point last_watchdog_ping_;
	FanDriver(const string &path, const unsigned int watchdog_timeout = 120);
public:
	FanDriver() : watchdog_(0) {}
	bool is_default() { return path_.length() == 0; }
	virtual ~FanDriver() = default;
	virtual void set_speed(const string &level);
	virtual void init() const {}
	virtual void ping_watchdog_and_depulse(const string &level) {};
};


class TpFanDriver : public FanDriver {
public:
	TpFanDriver(const string &path);
	~TpFanDriver() override;
	void set_watchdog(const unsigned int timeout);
	void set_depulse(float duration);
	void init() const override;
	void set_speed(const string &level);
	virtual void ping_watchdog_and_depulse(const string &level) override;
};


class HwmonFanDriver : public FanDriver {
public:
	HwmonFanDriver(const string &path);
	~HwmonFanDriver() override;
	void init() const override;
};


class SensorDriver {
protected:
	string path_;
	SensorDriver(string path);
	SensorDriver() : num_temps_(0) {}
	std::vector<int> correction_;
public:
	virtual ~SensorDriver() = default;
	virtual void read_temps() const = 0;
	unsigned int num_temps() const { return num_temps_; }
	void set_correction(const std::vector<int> &correction);
	void set_num_temps(unsigned int n);
private:
	unsigned int num_temps_;
};


class TpSensorDriver : public SensorDriver {
public:
	TpSensorDriver(string path);
	void read_temps() const override;
private:
	std::char_traits<char>::off_type skip_bytes_;
};


class HwmonSensorDriver : public SensorDriver {
public:
	HwmonSensorDriver(string path);
	void read_temps() const override;
};


#ifdef USE_ATASMART
class AtasmartSensorDriver : public SensorDriver {
public:
	AtasmartSensorDriver(string device_path);
	~AtasmartSensorDriver();
	void read_temps() const override;
private:
	SkDisk *disk_;
};
#endif /* USE_ATASMART */


#ifdef USE_NVML
class NvmlSensorDriver : public SensorDriver {
public:
	NvmlSensorDriver(string bus_id);
	~NvmlSensorDriver();
	void read_temps() const override;
private:
	nvmlDevice_t device_;
	void *nvml_so_handle_;

	// Pointers to dynamically loaded functions from libnvidia-ml.so
	nvmlReturn_t (*dl_nvmlInit_v2)();
	nvmlReturn_t (*dl_nvmlDeviceGetHandleByPciBusId_v2)(const char *, nvmlDevice_t *);
	nvmlReturn_t (*dl_nvmlDeviceGetName)(nvmlDevice_t, char *, unsigned int);
	nvmlReturn_t (*dl_nvmlDeviceGetTemperature)(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int *);
	nvmlReturn_t (*dl_nvmlShutdown)();

};
#endif /* USE_NVML */


}

#endif /* THINKFAN_DRIVERS_H_ */
