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

#include "error.h"
#include "drivers.h"
#include "message.h"
#include "config.h"

#include <fstream>
#include <cstring>
#include <thread>
#include <typeinfo>

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
	std::ofstream f_out(path_);
	if(!(f_out << level << std::flush)) {
		int err = errno;
		if (err == EPERM)
			throw SystemError(MSG_FAN_EPERM(path_));
		else
			throw IOerror(MSG_FAN_CTRL(level, path_), err);
	}
}


bool FanDriver::operator == (const FanDriver &other) const
{
	return typeid(*this) == typeid(other)
			&& this->path_ == other.path_
			&& this->depulse_ == other.depulse_
			&& this->watchdog_ == other.watchdog_;
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
	std::fstream f(path_);
	if (!(f.is_open() && f.good()))
		throw IOerror(MSG_FAN_INIT(path_), errno);

	std::string line;
	line.resize(256);

	while (f.getline(&*line.begin(), 255)) {
		if (f.fail())
			throw IOerror(MSG_FAN_INIT(path_), errno);
		if (line.rfind("level:") != string::npos) {
			// remember initial level, restore it in d'tor
			string::size_type offs = line.find_last_of(" \t") + 1;
			if (offs != string::npos) {
				// Cut of at bogus \000 char that may occur before EOL
				initial_state_ = line.substr(offs, line.find_first_of('\000') - offs);
			}
		}
		else if (line.rfind("commands:") != std::string::npos && line.rfind("level <level>") != std::string::npos) {
			ctrl_supported = true;
		}
	}

	if (!ctrl_supported)
		throw SystemError(MSG_FAN_MODOPTS);
	log(TF_DBG) << path_ << ": Saved initial state: " << initial_state_ << "." << flush;
}


TpFanDriver::~TpFanDriver()
{
	std::ofstream f(path_);
	if (!(f.is_open() && f.good()))
		throw IOerror(MSG_FAN_RESET(path_), errno);

	log(TF_DBG) << path_ << ": Restoring initial state: " << initial_state_ << "." << flush;
	if (!(f << "level " << initial_state_ << std::flush))
		throw IOerror(MSG_FAN_RESET(path_), errno);
}


void TpFanDriver::set_watchdog(const unsigned int timeout)
{ watchdog_ = std::chrono::duration<unsigned int>(timeout); }


void TpFanDriver::set_depulse(float duration)
{ depulse_ = std::chrono::duration<float>(duration); }


void TpFanDriver::set_speed(const Level *level)
{
	FanDriver::set_speed(level->str());
	last_watchdog_ping_ = std::chrono::system_clock::now();
}


void TpFanDriver::ping_watchdog_and_depulse(const Level *level)
{
	if (depulse_ > std::chrono::milliseconds(0)) {
		FanDriver::set_speed("level disengaged");
		std::this_thread::sleep_for(depulse_);
		set_speed(level);
	}
	else if (last_watchdog_ping_ + watchdog_ + sleeptime <= std::chrono::system_clock::now())
		set_speed(level);
}


void TpFanDriver::init() const
{
	std::fstream f(path_);
	if (!(f.is_open() && f.good()))
		throw IOerror(MSG_FAN_INIT(path_), errno);

	if (!(f << "watchdog " << watchdog_.count() << std::flush))
		throw IOerror(MSG_FAN_INIT(path_), errno);
}


/*----------------------------------------------------------------------------
| HwmonFanDriver: Driver for PWM fans, typically somewhere in sysfs.         |
----------------------------------------------------------------------------*/

HwmonFanDriver::HwmonFanDriver(const std::string &path)
: FanDriver(path, 0)
{
	std::ifstream f(path_ + "_enable");
	if (!(f.is_open() && f.good()))
		throw IOerror(MSG_FAN_INIT(path_), errno);

	std::string line;
	line.resize(64);
	if (!f.getline(&*line.begin(), 63))
		throw IOerror(MSG_FAN_INIT(path_), errno);
	initial_state_ = line;
	log(TF_DBG) << path_ << ": Saved initial state: " << initial_state_ << "." << flush;
}


HwmonFanDriver::~HwmonFanDriver()
{
	std::ofstream f(path_ + "_enable");
	if (!(f.is_open() && f.good()))
		throw IOerror(MSG_FAN_RESET(path_), errno);
	log(TF_DBG) << path_ << ": Restoring initial state: " << initial_state_ << "." << flush;
	if (!(f << initial_state_ << std::flush))
		throw IOerror(MSG_FAN_RESET(path_), errno);
}


void HwmonFanDriver::init() const
{
	std::ofstream f(path_ + "_enable");
	if (!(f.is_open() && f.good()))
		throw IOerror(MSG_FAN_INIT(path_), errno);
	if (!(f << "1" << std::flush))
		throw IOerror(MSG_FAN_INIT(path_), errno);
}


void HwmonFanDriver::set_speed(const Level *level)
{
	try {
		FanDriver::set_speed(std::to_string(level->num()));
	} catch (IOerror &e) {
		if (e.code() == EINVAL) {
			// This happens when the hwmon kernel driver is reset to automatic control
			// e.g. after the system has woken up from suspend.
			// In that case, we need to re-initialize and try once more.
			init();
			FanDriver::set_speed(std::to_string(level->num()));
			log(TF_DBG) << "It seems we woke up from suspend. PWM fan driver had to be re-initialized." << flush;
		} else {
			throw;
		}
	}
}


/*----------------------------------------------------------------------------
| SensorDriver: The superclass of all hardware-specific sensor drivers       |
----------------------------------------------------------------------------*/

SensorDriver::SensorDriver(std::string path, std::vector<int> correction)
: path_(path),
  correction_(correction),
  num_temps_(0)
{
	std::ifstream f(path_);
	if (!(f.is_open() && f.good()))
		throw IOerror(MSG_FAN_INIT(path_), errno);
}


void SensorDriver::set_correction(const std::vector<int> &correction)
{
	correction_ = correction;
	check_correction_length();
}


void SensorDriver::set_num_temps(unsigned int n)
{
	num_temps_ = n;
	if (correction_.empty())
		correction_.resize(num_temps_, 0);
	else
		check_correction_length();
}


bool SensorDriver::operator == (const SensorDriver &other) const
{
	return typeid(*this) == typeid(other)
			&& this->correction_ == other.correction_
			&& this->path_ == other.path_;
}


void SensorDriver::check_correction_length()
{
	if (correction_.size() > num_temps())
		throw ConfigError(MSG_CONF_CORRECTION_LEN(path_, correction_.size(), num_temps()));
	else if (correction_.size() < num_temps_)
		log(TF_WRN) << MSG_CONF_CORRECTION_LEN(path_, correction_.size(), num_temps()) << flush;
}


/*----------------------------------------------------------------------------
| HwmonSensorDriver: A driver for sensors provided by other kernel drivers,  |
| typically somewhere in sysfs.                                              |
----------------------------------------------------------------------------*/

HwmonSensorDriver::HwmonSensorDriver(std::string path, std::vector<int> correction)
: SensorDriver(path, correction)
{ set_num_temps(1); }


void HwmonSensorDriver::read_temps() const
{
	std::ifstream f(path_);
	if (!(f.is_open() && f.good()))
		throw IOerror(MSG_T_GET(path_), errno);
	int tmp;
	if (!(f >> tmp))
		throw IOerror(MSG_T_GET(path_), errno);
	temp_state.add_temp(tmp/1000 + correction_[0]);
}


/*----------------------------------------------------------------------------
| TpSensorDriver: A driver for sensors provided by thinkpad_acpi, typically  |
| in /proc/acpi/ibm/thermal.                                                 |
----------------------------------------------------------------------------*/

const string TpSensorDriver::skip_prefix_("temperatures:");

TpSensorDriver::TpSensorDriver(std::string path, const std::vector<unsigned int> &temp_indices, std::vector<int> correction)
: SensorDriver(path, correction)
{
	std::ifstream f(path_);
	if (!(f.is_open() && f.good()))
		throw IOerror(MSG_SENSOR_INIT(path_), errno);

	int tmp;
	unsigned int count = 0;

	string skip;
	skip.resize(skip_prefix_.size());

	if (!f.get(&*skip.begin(), static_cast<std::streamsize>(skip_prefix_.size() + 1)))
		throw IOerror(MSG_SENSOR_INIT(path_), errno);

	if (skip == skip_prefix_)
		skip_bytes_ = f.tellg();
	else
		throw SystemError(path_ + ": Unknown file format.");

	while (!(f.eof() || f.fail())) {
		f >> tmp;
		if (f.bad())
			throw IOerror(MSG_SENSOR_INIT(path_), errno);
		if (!f.fail())
			++count;
	}

	if (temp_indices.size() > 0) {
		if (temp_indices.size() > count)
			throw ConfigError("Config specifies " + std::to_string(temp_indices.size())
		                      + " temperature inputs in " + path
		                      + ", but there are only " + std::to_string(count) + ".");

		set_num_temps(static_cast<unsigned int>(temp_indices.size()));

		in_use_ = std::vector<bool>(count, false);
		for (unsigned int i : temp_indices)
			in_use_[i] = true;
	}
	else {
		in_use_ = std::vector<bool>(count, true);
		set_num_temps(count);
	}
}


TpSensorDriver::TpSensorDriver(std::string path, std::vector<int> correction)
	: TpSensorDriver(path, {}, correction)
{}


void TpSensorDriver::read_temps() const
{
	std::ifstream f(path_);
	if (!(f.is_open() && f.good()))
		throw IOerror(MSG_T_GET(path_), errno);

	f.seekg(skip_bytes_);
	if (f.fail())
		throw IOerror(MSG_T_GET(path_), errno);

	unsigned int tidx = 0;
	int tmp;
	while (!f.eof()) {
		f >> tmp;
		if (f.bad())
			throw IOerror(MSG_T_GET(path_), errno);
		if (!f.fail() && in_use_[tidx])
			temp_state.add_temp(tmp + correction_[tidx++]);
	}
}


#ifdef USE_ATASMART
/*----------------------------------------------------------------------------
| AtssmartSensorDriver: Reads temperatures from hard disks using S.M.A.R.T.  |
| via device files like /dev/sda.                                            |
----------------------------------------------------------------------------*/

AtasmartSensorDriver::AtasmartSensorDriver(string device_path, std::vector<int> correction)
: SensorDriver(device_path, correction)
{
	if (sk_disk_open(device_path.c_str(), &disk_) < 0) {
		string msg = std::strerror(errno);
		throw SystemError("sk_disk_open(" + device_path + "): " + msg);
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
		throw SystemError("sk_disk_check_sleep_mode(" + path_ + "): " + msg);
	}

	if (unlikely(disk_sleeping)) {
		temp_state.add_temp(0);
	}
	else {
		uint64_t mKelvin;
		float tmp;

		if (unlikely(sk_disk_smart_read_data(disk_) < 0)) {
			string msg = strerror(errno);
			throw SystemError("sk_disk_smart_read_data(" + path_ + "): " + msg);
		}
		if (unlikely(sk_disk_smart_get_temperature(disk_, &mKelvin)) < 0) {
			string msg = strerror(errno);
			throw SystemError("sk_disk_smart_get_temperature(" + path_ + "): " + msg);
		}

		tmp = mKelvin / 1000.0f;
		tmp -= 273.15f;

		if (unlikely(tmp > std::numeric_limits<int>::max() || tmp < std::numeric_limits<int>::min())) {
			throw SystemError(MSG_T_GET(path_) + std::to_string(tmp) + " isn't a valid temperature.");
		}

		temp_state.add_temp(int(tmp));
	}
}
#endif /* USE_ATASMART */


#ifdef USE_NVML
/*----------------------------------------------------------------------------
| NvmlSensorDriver: Gets temperatures directly from GPUs supported by the    |
| nVidia Management Library that is included with the proprietary driver.    |
----------------------------------------------------------------------------*/

NvmlSensorDriver::NvmlSensorDriver(string bus_id, std::vector<int> correction)
: dl_nvmlInit_v2(nullptr),
  dl_nvmlDeviceGetHandleByPciBusId_v2(nullptr),
  dl_nvmlDeviceGetName(nullptr),
  dl_nvmlDeviceGetTemperature(nullptr),
  dl_nvmlShutdown(nullptr)
{
	if (!(nvml_so_handle_ = dlopen("libnvidia-ml.so", RTLD_LAZY))) {
		string msg = strerror(errno);
		throw SystemError("Failed to load NVML driver: " + msg);
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
		throw SystemError("Incompatible NVML driver.");

	set_correction(correction);

	nvmlReturn_t ret;
	string name, brand;
	name.resize(256);
	brand.resize(256);

	if ((ret = dl_nvmlInit_v2()))
		throw SystemError("Failed to initialize NVML driver. Error code (cf. nvml.h): " + std::to_string(ret));
	if ((ret = dl_nvmlDeviceGetHandleByPciBusId_v2(bus_id.c_str(), &device_)))
		throw SystemError("Failed to open PCI device " + bus_id + ". Error code (cf. nvml.h): " + std::to_string(ret));
	dl_nvmlDeviceGetName(device_, &*name.begin(), 255);
	log(TF_DBG) << "Initialized NVML sensor on " << name << " at PCI " << bus_id << "." << flush;
	set_num_temps(1);
}


NvmlSensorDriver::~NvmlSensorDriver()
{
	nvmlReturn_t ret;
	if ((ret = dl_nvmlShutdown()))
		throw SystemError("Failed to shutdown NVML driver. Error code (cf. nvml.h): " + std::to_string(ret));
	dlclose(nvml_so_handle_);
}

void NvmlSensorDriver::read_temps() const
{
	nvmlReturn_t ret;
	unsigned int tmp;
	if ((ret = dl_nvmlDeviceGetTemperature(device_, NVML_TEMPERATURE_GPU, &tmp)))
		throw SystemError(MSG_T_GET(path_) + "Error code (cf. nvml.h): " + std::to_string(ret));
	temp_state.add_temp(tmp);
}
#endif /* USE_NVML */



}



