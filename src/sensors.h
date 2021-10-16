#pragma once

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

#include "thinkfan.h"
#include "error.h"

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

namespace thinkfan {

class ExpectedError;

class SensorDriver {
protected:
	SensorDriver(string path, bool optional, std::vector<int> correction = {});
	SensorDriver(bool optional);
public:
	virtual ~SensorDriver() noexcept(false);
	unsigned int num_temps() const { return num_temps_; }
	void set_correction(const std::vector<int> &correction);
	void set_num_temps(unsigned int n);
	bool operator == (const SensorDriver &other) const;
	void set_optional(bool);
	bool optional() const;
	const string &path() const;

	inline void read_temps(TemperatureState &global_temps) const
	{
		try {
			read_temps_(global_temps);
		} catch (SystemError &e) {
			sensor_lost(e, global_temps);
		} catch (IOerror &e) {
			sensor_lost(e, global_temps);
		} catch (std::ios_base::failure &e) {
			sensor_lost(IOerror(e.what(), THINKFAN_IO_ERROR_CODE(e)), global_temps);
		}
	}

protected:
	virtual void read_temps_(TemperatureState &global_temps) const = 0;

	string path_;
	std::vector<int> correction_;

private:
	unsigned int num_temps_;
	bool optional_;
	void check_correction_length();

	inline void sensor_lost(const ExpectedError &e, TemperatureState &global_temps) const
	{
		if (this->optional() || tolerate_errors)
			log(TF_INF) << SensorLost(e).what();
		else
			error<SensorLost>(e);
		global_temps.add_temp(-128);
	}
};


class TpSensorDriver : public SensorDriver {
public:
	TpSensorDriver(string path, bool optional, std::vector<int> correction = {});
	TpSensorDriver(string path, bool optional, const std::vector<unsigned int> &temp_indices, std::vector<int> correction = {});
protected:
	virtual void read_temps_(TemperatureState &global_temps) const override;
private:
	std::char_traits<char>::off_type skip_bytes_;
	static const string skip_prefix_;
	std::vector<bool> in_use_;
};


class HwmonSensorDriver : public SensorDriver {
public:
	HwmonSensorDriver(string path, bool optional, std::vector<int> correction = {});
protected:
	virtual void read_temps_(TemperatureState &global_temps) const override;
};


#ifdef USE_ATASMART
class AtasmartSensorDriver : public SensorDriver {
public:
	AtasmartSensorDriver(string device_path, bool optional, std::vector<int> correction = {});
	virtual ~AtasmartSensorDriver();
protected:
	virtual void read_temps_(TemperatureState &global_temps) const override;
private:
	SkDisk *disk_;
};
#endif /* USE_ATASMART */


#ifdef USE_NVML
class NvmlSensorDriver : public SensorDriver {
public:
	NvmlSensorDriver(string bus_id, bool optional, std::vector<int> correction = {});
	virtual ~NvmlSensorDriver() noexcept(false) override;
protected:
	virtual void read_temps_(TemperatureState &global_temps) const override;
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
	LMSensorsDriver(string chip_name, std::vector<string> feature_names,
	                bool optional, std::vector<int> correction = {});
	virtual ~LMSensorsDriver();

protected:
	virtual void read_temps_(TemperatureState &global_temps) const override;

	// LM sensors helpers.
	static void ensure_lm_sensors_is_initialized();
	static void initialize_lm_sensors(int* result);
	static const ::sensors_chip_name* find_chip_by_name(
		const string& chip_name);
	static const ::sensors_feature* find_feature_by_name(
		const ::sensors_chip_name& chip, const string& chip_name,
		const string& feature_name);
	static string get_chip_name(const ::sensors_chip_name& chip);

	// LM sensors call backs.
	static void parse_error_call_back(const char *err, int line_no);
	static void parse_error_wfn_call_back(const char *err, const char *file_name, int line_no);
	static void fatal_error_call_back(const char *proc, const char *err);

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

