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
	seconds watchdog_;
	secondsf depulse_;
	std::chrono::system_clock::time_point last_watchdog_ping_;
	FanDriver(const string &path, const unsigned int watchdog_timeout = 120);
public:
	FanDriver() : watchdog_(0) {}
	bool is_default() { return path_.length() == 0; }
	virtual ~FanDriver() noexcept(false) {}
	virtual void init() {}
	virtual void set_speed(const string &level);
	virtual void set_speed(const Level &level) = 0;
	virtual void ping_watchdog_and_depulse(const Level &) {}
	bool operator == (const FanDriver &other) const;
};


class TpFanDriver : public FanDriver {
public:
	TpFanDriver(const string &path);
	virtual ~TpFanDriver() noexcept(false) override;
	void set_watchdog(const unsigned int timeout);
	void set_depulse(float duration);
	virtual void init() override;
	virtual void set_speed(const Level &level) override;
	virtual void ping_watchdog_and_depulse(const Level &level) override;
};


class HwmonFanDriver : public FanDriver {
public:
	HwmonFanDriver(const string &path);
	virtual ~HwmonFanDriver() noexcept(false) override;
	virtual void init() override;
	virtual void set_speed(const Level &level) override;
};


class SensorDriver {
protected:
	SensorDriver(string path, bool optional, std::vector<int> correction = {});
	SensorDriver(bool optional);
public:
	virtual ~SensorDriver() noexcept(false);
	virtual void read_temps() const = 0;
	unsigned int num_temps() const { return num_temps_; }
	void set_correction(const std::vector<int> &correction);
	void set_num_temps(unsigned int n);
	bool operator == (const SensorDriver &other) const;
	void set_optional(bool);
	bool optional() const;
	const string &path() const;
protected:
	string path_;
	std::vector<int> correction_;
private:
	unsigned int num_temps_;
	bool optional_;
	void check_correction_length();
};


class TpSensorDriver : public SensorDriver {
public:
	TpSensorDriver(string path, bool optional, std::vector<int> correction = {});
	TpSensorDriver(string path, bool optional, const std::vector<unsigned int> &temp_indices, std::vector<int> correction = {});
	virtual void read_temps() const override;
private:
	std::char_traits<char>::off_type skip_bytes_;
	static const string skip_prefix_;
	std::vector<bool> in_use_;
};


class HwmonSensorDriver : public SensorDriver {
public:
	HwmonSensorDriver(string path, bool optional, std::vector<int> correction = {});
	virtual void read_temps() const override;
};


#ifdef USE_ATASMART
class AtasmartSensorDriver : public SensorDriver {
public:
	AtasmartSensorDriver(string device_path, bool optional, std::vector<int> correction = {});
	virtual ~AtasmartSensorDriver();
	virtual void read_temps() const override;
private:
	SkDisk *disk_;
};
#endif /* USE_ATASMART */


#ifdef USE_NVML
class NvmlSensorDriver : public SensorDriver {
public:
	NvmlSensorDriver(string bus_id, bool optional, std::vector<int> correction = {});
	virtual ~NvmlSensorDriver() noexcept(false) override;
	virtual void read_temps() const override;
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
