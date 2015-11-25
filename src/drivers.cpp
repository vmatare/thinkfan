/********************************************************************
 * drivers.cpp: Interface to the kernel drivers.
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

#include "drivers.h"
#include "message.h"
#include "config.h"
#include "error.h"

#include <fstream>
#include <cstring>
#include <thread>

#ifdef USE_NVML
#include <dlfcn.h>
#endif

namespace thinkfan {


/*----------------------------------------------------------------------------
| FanDriver: Superclass of TpFanDriver and HwmonFanDriver. Can set the speed |
| on its own since an implementation-specific string representation is       |
| provided by its subclasses.                                                |
----------------------------------------------------------------------------*/

FanDriver::FanDriver(const std::string &path, const unsigned int watchdog_timeout)
: path_(path),
  watchdog_(watchdog_timeout),
  depulse_(0)
{}


void FanDriver::set_speed(const string &level)
{
	try {
		std::ofstream f_out(path_);
		f_out.exceptions(f_out.failbit | f_out.badbit);
		f_out << level << std::flush;
	} catch (std::ios_base::failure &e) {
		string msg = std::strerror(errno);
		fail(TF_ERR) << SystemError(MSG_FAN_CTRL(level, path_) + msg) << flush;
	}
}


/*----------------------------------------------------------------------------
| TpFanDriver: Driver for fan control via thinkpad_acpi, typically in        |
| /proc/acpi/ibm/fan. Supports fan watchdog and depulsing (an alleged remedy |
| for noise oscillation with old & worn-out fans).                           |
----------------------------------------------------------------------------*/

TpFanDriver::TpFanDriver(const std::string &path)
: FanDriver(path, 120)
{
	bool ctrl_supported = false;
	try {
		std::fstream f(path_);
		f.exceptions(f.failbit | f.badbit);
		std::string line;
		line.resize(256);

		f.exceptions(f.badbit);
		while (	f.getline(&*line.begin(), 255)) {
			if (line.rfind("level:") != string::npos) {
				// remember initial level, restore it in d'tor
				initial_state_ = line.substr(line.find_last_of(" \t") + 1);
			}
			else if (line.rfind("commands:") != std::string::npos && line.rfind("level <level>") != std::string::npos) {
				ctrl_supported = true;
			}
		}
	} catch (std::ios_base::failure &e) {
		string msg = std::strerror(errno);
		fail(TF_ERR) << SystemError(MSG_FAN_INIT(path_) + msg) << flush;
	}

	if (!ctrl_supported) fail(TF_ERR) << SystemError(MSG_FAN_MODOPTS) << flush;
}


TpFanDriver::~TpFanDriver()
{
	try {
		std::ofstream f(path_);
		f.exceptions(f.failbit | f.badbit);
		f << "level " << initial_state_ << std::endl;
	} catch (std::ios_base::failure &e) {
		string msg = std::strerror(errno);
		fail(TF_ERR) << SystemError(MSG_FAN_RESET(path_) + msg) << flush;
	}
}


void TpFanDriver::set_watchdog(const unsigned int timeout)
{ watchdog_ = std::chrono::duration<unsigned int>(timeout); }


void TpFanDriver::set_depulse(float duration)
{ depulse_ = std::chrono::duration<float>(duration); }


void TpFanDriver::set_speed(const string &level)
{
	FanDriver::set_speed(level);
	last_watchdog_ping_ = std::chrono::system_clock::now();
}


void TpFanDriver::ping_watchdog_and_depulse(const string &level)
{
	if (depulse_ > std::chrono::milliseconds(0)) {
		set_speed("level disengaged");
		std::this_thread::sleep_for(depulse_);
		set_speed(level);
	}
	else if (last_watchdog_ping_ + watchdog_ + sleeptime >= std::chrono::system_clock::now())
		set_speed(level);
}


void TpFanDriver::init() const
{
	try {
		std::fstream f(path_);
		f.exceptions(f.failbit | f.badbit);
		f << "watchdog " << watchdog_.count() << std::endl;
	} catch (std::ios_base::failure &e) {
		string msg = std::strerror(errno);
		fail(TF_ERR) << SystemError(MSG_FAN_INIT(path_) + msg) << flush;
	}
}


/*----------------------------------------------------------------------------
| HwmonFanDriver: Driver for PWM fans, typically somewhere in sysfs.         |
----------------------------------------------------------------------------*/

HwmonFanDriver::HwmonFanDriver(const std::string &path)
: FanDriver(path, 0)
{
	try {
		std::ifstream f(path_ + "_enable");
		f.exceptions(f.failbit | f.badbit);
		std::string line;
		line.resize(64);
		f.getline(&*line.begin(), 63);
		initial_state_ = line;
	} catch (std::ios_base::failure &e) {
		string msg = std::strerror(errno);
		fail(TF_ERR) << SystemError(MSG_FAN_INIT(path_) + msg) << flush;
	}
}


HwmonFanDriver::~HwmonFanDriver()
{
	try {
		std::ofstream f(path_ + "_enable");
		f.exceptions(f.failbit | f.badbit);
		f << initial_state_ << std::endl;
	} catch (std::ios_base::failure &e) {
		string msg = std::strerror(errno);
		fail(TF_WRN) << SystemError(MSG_FAN_RESET(path_) + msg) << flush;
	}
}


void HwmonFanDriver::init() const
{
	try {
		std::ofstream f(path_ + "_enable");
		f.exceptions(f.failbit | f.badbit);
		f << "1" << std::endl;
	} catch (std::ios_base::failure &e) {
		string msg = std::strerror(errno);
		fail(TF_ERR) << SystemError(MSG_FAN_INIT(path_) + msg) << flush;
	}
}


/*----------------------------------------------------------------------------
| SensorDriver: The superclass of all hardware-specific sensor drivers       |
----------------------------------------------------------------------------*/

SensorDriver::SensorDriver(std::string path)
: path_(path),
  num_temps_(0)
{
	try {
		std::ifstream f(path_);
		f.exceptions(f.failbit | f.badbit);
	} catch (std::ios_base::failure &e) {
		string msg = std::strerror(errno);
		fail(TF_ERR) << SystemError(MSG_FAN_INIT(path_) + msg) << flush;
	}
}


void SensorDriver::set_correction(const std::vector<int> &correction)
{
	if (correction.size() > num_temps())
		fail(TF_WRN) << ConfigError(MSG_CONF_CORRECTION_LEN(path_, correction.size(), num_temps_)) << flush;
	else if (correction.size() < num_temps())
		log(TF_WRN, TF_WRN) << MSG_CONF_CORRECTION_LEN(path_, correction.size(), num_temps_) << flush;
	correction_ = correction;
}


void SensorDriver::set_num_temps(unsigned int n)
{
	num_temps_ = n;
	correction_.resize(n, 0);
}


inline void update_tempstate(int correction) {
	*temp_state.temp_idx += correction;
	if (*temp_state.temp_idx > temp_state.tmax) {
		temp_state.b_tmax = temp_state.temp_idx;
		temp_state.tmax = *temp_state.temp_idx;
	}
	++temp_state.temp_idx;
}


/*----------------------------------------------------------------------------
| HwmonSensorDriver: A driver for sensors provided by other kernel drivers,  |
| typically somewhere in sysfs.                                              |
----------------------------------------------------------------------------*/

HwmonSensorDriver::HwmonSensorDriver(std::string path)
: SensorDriver(path)
{ set_num_temps(1); }


void HwmonSensorDriver::read_temps() const
{
	try {
		std::ifstream f(path_);
		f.exceptions(f.failbit | f.badbit);
		int tmp;
		f >> tmp;
		*temp_state.temp_idx = tmp / 1000;
		update_tempstate(correction_[0]);
	} catch (std::ios_base::failure &e) {
		string msg = std::strerror(errno);
		fail(TF_ERR) << SystemError(MSG_T_GET(path_) + msg) << flush;
	}
}


/*----------------------------------------------------------------------------
| TpSensorDriver: A driver for sensors provided by thinkpad_acpi, typically  |
| in /proc/acpi/ibm/thermal.                                                 |
----------------------------------------------------------------------------*/

TpSensorDriver::TpSensorDriver(std::string path)
: SensorDriver(path)
{
	try {
		std::ifstream f(path_);
		f.exceptions(f.failbit | f.badbit);
		int tmp;
		unsigned int count = 0;

		string skip;
		skip.resize(16);
		f.getline(&*skip.begin(), 15, ':');
		if (skip == "temperatures") {
			// another stdlib messup, can't simply add one to a pos_type...
			skip_bytes_ = f.tellg();
			++skip_bytes_;
		}
		else
			fail(TF_ERR) << SystemError(path_ + ": Unknown file format.") << flush;

		while (!f.eof()) {
			f >> tmp;
			++count;
		}
		set_num_temps(count);
	} catch (std::ios_base::failure &e) {
		string msg = std::strerror(errno);
		fail(TF_ERR) << SystemError(MSG_SENSOR_INIT(path_) + msg) << flush;
	}
}


void TpSensorDriver::read_temps() const
{
	try {
		std::ifstream f(path_);
		f.exceptions(f.failbit | f.badbit);
		f.seekg(skip_bytes_);

		unsigned int tidx = 0;
		while (!f.eof()) {
			f >> *temp_state.temp_idx;
			update_tempstate(correction_[tidx++]);
		}
	} catch (std::ios_base::failure &e) {
		string msg = std::strerror(errno);
		fail(TF_ERR) << MSG_T_GET(path_) << SystemError(msg) << flush;
	}
}


#ifdef USE_ATASMART
/*----------------------------------------------------------------------------
| AtssmartSensorDriver: Reads temperatures from hard disks using S.M.A.R.T.  |
| via device files like /dev/sda.                                            |
----------------------------------------------------------------------------*/

AtasmartSensorDriver::AtasmartSensorDriver(string device_path)
: SensorDriver(device_path)
{
	if (sk_disk_open(device_path.c_str(), &disk_) < 0) {
		string msg = std::strerror(errno);
		fail(TF_ERR) << SystemError("sk_disk_open(" + device_path + "): " + msg);
	}
	set_num_temps(1);
}


AtasmartSensorDriver::~AtasmartSensorDriver()
{ sk_disk_free(disk_); }


void AtasmartSensorDriver::read_temps() const
{
	SkBool disk_sleeping = false;

	if (unlikely(dnd_disk && (sk_disk_check_sleep_mode(disk_, &disk_sleeping) < 0))) {
		string msg = strerror(errno);
		fail(TF_ERR) << SystemError("sk_disk_check_sleep_mode(" + path_ + "): " + msg) << flush;
	}

	if (unlikely(disk_sleeping)) {
		*temp_state.temp_idx = 0;
		update_tempstate(correction_[0]);
	}
	else {
		uint64_t mKelvin;
		double tmp;

		if (unlikely(sk_disk_smart_read_data(disk_) < 0)) {
			string msg = strerror(errno);
			fail(TF_ERR) << SystemError("sk_disk_smart_read_data(" + path_ + "): " + msg) << flush;
		}
		if (unlikely(sk_disk_smart_get_temperature(disk_, &mKelvin)) < 0) {
			string msg = strerror(errno);
			fail(TF_ERR) << SystemError("sk_disk_smart_get_temperature(" + path_ + "): " + msg) << flush;
		}

		tmp = mKelvin / 1000.0f;
		tmp -= 273.15f;

		if (unlikely(tmp > std::numeric_limits<int>::max() || tmp < std::numeric_limits<int>::min())) {
			fail(TF_ERR) << MSG_T_GET(path_) << SystemError(std::to_string(tmp) + " isn't a valid temperature.") << flush;
		}

		*temp_state.temp_idx = tmp;
		update_tempstate(correction_[0]);
	}
}
#endif /* USE_ATASMART */


#ifdef USE_NVML
/*----------------------------------------------------------------------------
| NvmlSensorDriver: Gets temperatures directly from GPUs supported by the    |
| nVidia Management Library that is included with the proprietary driver.    |
----------------------------------------------------------------------------*/

NvmlSensorDriver::NvmlSensorDriver(string bus_id)
: dl_nvmlInit_v2(nullptr),
  dl_nvmlDeviceGetHandleByPciBusId_v2(nullptr),
  dl_nvmlDeviceGetName(nullptr),
  dl_nvmlDeviceGetTemperature(nullptr),
  dl_nvmlShutdown(nullptr)
{
	if (!(nvml_so_handle_ = dlopen("libnvidia-ml.so", RTLD_LAZY))) {
		string msg = strerror(errno);
		fail(TF_ERR) << SystemError("Failed to load NVML driver: " + msg) << flush;
	}

	/* Apparently GCC doesn't want to cast to function pointers, so we have to do
	 * this kind of weird stuff.
	 * See http://stackoverflow.com/questions/1096341/function-pointers-casting-in-c
	 */
	*reinterpret_cast<void **>(&dl_nvmlInit_v2) = dlsym(nvml_so_handle_, "nvmlInit_v2");
	*reinterpret_cast<void **>(&dl_nvmlDeviceGetHandleByPciBusId_v2) = dlsym(
			nvml_so_handle_, "nvmlDeviceGetHandleByPciBusId_v2");
	*reinterpret_cast<void **>(&dl_nvmlDeviceGetName) = dlsym(nvml_so_handle_, "nvmlDeviceGetName");
	*reinterpret_cast<void **>(&dl_nvmlDeviceGetTemperature) = dlsym(nvml_so_handle_, "nvmlDeviceGetTemperature");
	*reinterpret_cast<void **>(&dl_nvmlShutdown) = dlsym(nvml_so_handle_, "nvmlShutdown");

	if (!(dl_nvmlDeviceGetHandleByPciBusId_v2 && dl_nvmlDeviceGetName &&
			dl_nvmlDeviceGetTemperature && dl_nvmlInit_v2 && dl_nvmlShutdown))
		fail(TF_ERR) << SystemError("Incompatible NVML driver.") << flush;

	nvmlReturn_t ret;
	string name, brand;
	name.resize(256);
	brand.resize(256);

	if ((ret = dl_nvmlInit_v2()))
		fail(TF_ERR)
		<< SystemError("Failed to initialize NVML driver. Error code (cf. nvml.h): " + std::to_string(ret))
		<< flush;
	if ((ret = dl_nvmlDeviceGetHandleByPciBusId_v2(bus_id.c_str(), &device_)))
		fail(TF_ERR)
		<< SystemError("Failed to open PCI device " + bus_id + ". Error code (cf. nvml.h): " + std::to_string(ret))
		<< flush;
	dl_nvmlDeviceGetName(device_, &*name.begin(), 255);
	log(TF_DBG, TF_DBG) << "Initialized NVML sensor on " << name << " at PCI " << bus_id << "." << flush;
	set_num_temps(1);
}


NvmlSensorDriver::~NvmlSensorDriver()
{
	nvmlReturn_t ret;
	if ((ret = dl_nvmlShutdown()))
		log(TF_ERR, TF_ERR) << "Failed to shutdown NVML driver. Error code (cf. nvml.h): " << ret << flush;
	dlclose(nvml_so_handle_);
	delete dlerror();
}

void NvmlSensorDriver::read_temps() const
{
	nvmlReturn_t ret;
	unsigned int tmp;
	if ((ret = dl_nvmlDeviceGetTemperature(device_, NVML_TEMPERATURE_GPU, &tmp)))
		fail(TF_ERR)
		<< SystemError(MSG_T_GET(path_) + "Error code (cf. nvml.h): " + std::to_string(ret))
		<< flush;
	*temp_state.temp_idx = tmp;
	update_tempstate(correction_[0]);
}
#endif /* USE_NVML */



}



