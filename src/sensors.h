#pragma once

/********************************************************************
 * config.h: Config data structures and consistency checking.
 * (C) 2015, Victor Mataré
 *     2021, Koutheir Attouchi
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

#include "thinkfan.h"
#include "error.h"
#include "driver.h"
#include "hwmon.h"

#ifdef USE_ATASMART
#include <atasmart.h>
#endif /* USE_ATASMART */

#ifdef USE_NVML
#include <nvidia/gdk/nvml.h>
#endif /* USE_NVML */

#ifdef USE_LM_SENSORS
#include <sensors/sensors.h>
#include <sensors/error.h>
#include <atomic>
#include <mutex>
#endif /* USE_LM_SENSORS */

#include <functional>
#include <optional>

namespace thinkfan {

class ExpectedError;

template<class T>
using optional = std::optional<T>;


class SensorDriver : public Driver {
protected:
	SensorDriver(unsigned int max_errors, opt<const string> &&path, bool optional, const vector<int> &correction = {});

public:
	virtual ~SensorDriver() noexcept(false);
	unsigned int num_temps() const { return num_temps_; }
	void set_correction(const vector<int> &correction);
	bool operator == (const SensorDriver &other) const;

	void read_temps();
	void init_temp_state_ref(TemperatureState::Ref &&);

protected:
	virtual void init() override;
	void set_num_temps(unsigned int n);
	virtual void skip_io_error(const ExpectedError &e) override;
	virtual void read_temps_() = 0;

	vector<int> correction_;
	TemperatureState::Ref temp_state_;

	/** @brief Protocol: Throw SensorLost(e) or nothing
	 *  @param e The original error */
private:
	unsigned int num_temps_;
	void check_correction_length();
};


class HwmonSensorDriver : public SensorDriver, public HwmonInterface {
public:
	HwmonSensorDriver(
		const string &path,
		bool optional,
		const vector<int> &correction = {},
		unsigned int max_errors = 0
	);
	HwmonSensorDriver(
		const string &base_path,
		opt<const string> &&name,
		bool optional,
		opt<unsigned int> &&index,
		const vector<int> &correction,
		unsigned int max_errors
	);

protected:
	virtual void read_temps_() override;
	virtual string lookup() override;
};


class TpSensorDriver : public SensorDriver {
public:
	TpSensorDriver(
		string path,
		bool optional,
		const vector<int> &correction = {},
		unsigned int max_errors = 0
	);
	TpSensorDriver(
		string path,
		bool optional,
		const vector<unsigned int> &temp_indices,
		const vector<int> &correction = {},
		unsigned int max_errors = 0
	);

protected:
	virtual void init() override;
	virtual void read_temps_() override;
	virtual string lookup() override;

private:
	std::char_traits<char>::off_type skip_bytes_;
	static const string skip_prefix_;
	vector<bool> in_use_;
	const vector<unsigned int> &temp_indices_;
};


#ifdef USE_ATASMART
class AtasmartSensorDriver : public SensorDriver {
public:
	AtasmartSensorDriver(string device_path, bool optional, vector<int> correction = {}, unsigned int max_errors = 0);
	virtual ~AtasmartSensorDriver();
protected:
	virtual void init() override;
	virtual void read_temps_() override;
	virtual string lookup() override;
private:
	SkDisk *disk_;
};
#endif /* USE_ATASMART */


#ifdef USE_NVML
class NvmlSensorDriver : public SensorDriver {
public:
	NvmlSensorDriver(string bus_id, bool optional, vector<int> correction = {}, unsigned int max_errors = 0);
	virtual ~NvmlSensorDriver() noexcept(false) override;
protected:
	virtual void init() override;
	virtual void read_temps_() override;
	virtual string lookup() override;
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


#ifdef USE_LM_SENSORS

class LMSensorsDriver : public SensorDriver {
public:
	LMSensorsDriver(
		string chip_name,
		std::vector<string> feature_names,
		bool optional,
		std::vector<int> correction = {},
		unsigned int max_errors = 0
	);
	virtual ~LMSensorsDriver();

protected:
	virtual void init() override;
	virtual void read_temps_() override;
	virtual string lookup() override;

	// LM sensors helpers.
	static void initialize_lm_sensors();
	static const ::sensors_chip_name *find_chip_by_name(const string& chip_name);

	static const ::sensors_feature *find_feature_by_name(
		const ::sensors_chip_name &chip,
		const string &feature_name
	);

	static string get_chip_name(const ::sensors_chip_name &chip);

	// LM sensors call backs.
	static void parse_error_callback(const char *err, int line_no);
	static void parse_error_wfn_callback(const char *err, const char *file_name, int line_no);
	static void fatal_error_callback(const char *proc, const char *err);

private:
	const string chip_name_;
	const ::sensors_chip_name* chip_;

	const std::vector<string> feature_names_;
	std::vector<const ::sensors_feature*> features_;
	std::vector<const ::sensors_subfeature*> sub_features_;

	static std::once_flag lm_sensors_once_init_;
};

#endif /* USE_LM_SENSORS */

}

