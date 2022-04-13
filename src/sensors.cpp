/********************************************************************
 * drivers.cpp: Interface to the kernel drivers.
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

#include "sensors.h"
#include "error.h"
#include "message.h"
#include "config.h"

#include <fstream>
#include <cstring>
#include <thread>
#include <typeinfo>
#include <cmath>

#ifdef USE_NVML
#include <dlfcn.h>
#endif

namespace thinkfan {


/*----------------------------------------------------------------------------
| SensorDriver: The superclass of all hardware-specific sensor drivers       |
----------------------------------------------------------------------------*/

SensorDriver::SensorDriver(opt<const string> path, bool optional, opt<vector<int>> correction, opt<unsigned int> max_errors)
: Driver(std::forward<opt<const string>>(path), optional, max_errors.value_or(0))
, correction_(correction.value_or(vector<int>()))
, num_temps_(0)
{}

void SensorDriver::init()
{
	std::ifstream f(path());
	if (!(f.is_open() && f.good()))
		throw IOerror(path() + ": ", errno);
}



SensorDriver::~SensorDriver() noexcept(false)
{}


void SensorDriver::set_correction(const vector<int> &correction)
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
	&& this->path() == other.path();
}


void SensorDriver::read_temps()
{
	temp_state_.restart();
	robust_io(&SensorDriver::read_temps_);
}

void SensorDriver::init_temp_state_ref(TemperatureState::Ref &&ref)
{ temp_state_ = std::move(ref); }


void SensorDriver::check_correction_length()
{
	if (correction_.size() > num_temps())
		throw ConfigError(MSG_CONF_CORRECTION_LEN(path(), correction_.size(), num_temps()));
	else if (correction_.size() < num_temps_)
		log(TF_WRN) << MSG_CONF_CORRECTION_LEN(path(), correction_.size(), num_temps()) << flush;
}


void SensorDriver::skip_io_error(const ExpectedError &e)
{
	if (this->optional()) {
		log(TF_INF) << DriverLost(e).what();
		// Completely ignore sensor. optional says we're good without it
		temp_state_.add_temp(-128);
	}
	else if (tolerate_errors) {
		log(TF_NFY) << DriverLost(e).what();
		// Read error on wakeup: keep last temp
		temp_state_.skip_temp();
	}
	else {
		log(TF_NFY) << "Ignoring Error " << errors() << "/" << max_errors()
		<< " on " << path() << ": " << e.what();

		// Other error the user said is acceptable: keep last temp
		temp_state_.skip_temp();
	}
}




/*----------------------------------------------------------------------------
| HwmonSensorDriver: A driver for sensors provided by other kernel drivers,  |
| typically somewhere in sysfs.                                              |
----------------------------------------------------------------------------*/

HwmonSensorDriver::HwmonSensorDriver(
	const string &path,
	bool optional,
	opt<int> correction,
	opt<unsigned int> max_errors
)
: SensorDriver(path, optional, correction ? vector<int>{*correction} : vector<int>{}, max_errors)
, HwmonInterface(path, nullopt, nullopt)
{ set_num_temps(1); }

HwmonSensorDriver::HwmonSensorDriver(
	const string &base_path,
	opt<const string> name,
	bool optional,
	opt<unsigned int> index,
	opt<int> correction,
	opt<unsigned int> max_errors
)
: SensorDriver(std::nullopt, optional, correction ? vector<int>{*correction} : vector<int>{}, max_errors)
, HwmonInterface(base_path, name, index)
{ set_num_temps(1); }


void HwmonSensorDriver::read_temps_()
{
	std::ifstream f(path());
	if (!(f.is_open() && f.good()))
		throw IOerror(MSG_T_GET(path()), errno);
	int tmp;
	if (!(f >> tmp))
		throw IOerror(MSG_T_GET(path()), errno);
	temp_state_.add_temp(tmp/1000 + correction_[0]);
}


string HwmonSensorDriver::lookup()
{ return HwmonInterface::lookup<HwmonSensorDriver>(); }


/*----------------------------------------------------------------------------
| TpSensorDriver: A driver for sensors provided by thinkpad_acpi, typically  |
| in /proc/acpi/ibm/thermal.                                                 |
----------------------------------------------------------------------------*/

const string TpSensorDriver::skip_prefix_("temperatures:");

TpSensorDriver::TpSensorDriver(
	std::string path,
	bool optional,
	opt<vector<unsigned int>> temp_indices,
	opt<vector<int>> correction,
	opt<unsigned int> max_errors
)
: SensorDriver(path, optional, correction, max_errors)
, temp_indices_(temp_indices)
{
	if (temp_indices_)
		set_num_temps(static_cast<unsigned int>(temp_indices_->size()));
}


void TpSensorDriver::init()
{
	std::ifstream f(path());
	if (!(f.is_open() && f.good()))
		throw IOerror(MSG_SENSOR_INIT(path()), errno);

	int tmp;
	unsigned int count = 0;

	string skip;
	skip.resize(skip_prefix_.size());

	if (!f.get(&*skip.begin(), static_cast<std::streamsize>(skip_prefix_.size() + 1)))
		throw IOerror(MSG_SENSOR_INIT(path()), errno);

	if (skip == skip_prefix_)
		skip_bytes_ = f.tellg();
	else
		throw SystemError(path() + ": Unknown file format.");

	while (!(f.eof() || f.fail())) {
		f >> tmp;
		if (f.bad())
			throw IOerror(MSG_T_GET(path()), errno);
		if (!f.fail())
			++count;
	}

	if (temp_indices_) {
		if (temp_indices_->size() > count)
			throw ConfigError(
				"Config specifies " + std::to_string(temp_indices_->size())
				+ " temperature inputs in " + path()
				+ ", but there are only " + std::to_string(count) + "."
			);


		in_use_ = vector<bool>(count, false);
		for (unsigned int i : *temp_indices_)
			in_use_[i] = true;
	}
	else {
		in_use_ = vector<bool>(count, true);
		set_num_temps(count);
	}
}


void TpSensorDriver::read_temps_()
{
	std::ifstream f(path());
	if (!(f.is_open() && f.good()))
		throw IOerror(MSG_T_GET(path()), errno);

	f.seekg(skip_bytes_);
	if (f.fail())
		throw IOerror(MSG_T_GET(path()), errno);

	unsigned int tidx = 0;
	unsigned int cidx = 0;
	int tmp;
	while (!(f.eof() || f.fail())) {
		f >> tmp;
		if (f.bad())
			throw IOerror(MSG_T_GET(path()), errno);
		if (!f.fail() && in_use_[tidx++])
			temp_state_.add_temp(tmp + correction_[cidx++]);
	}
}


string TpSensorDriver::lookup()
{ return path(); }


#ifdef USE_ATASMART
/*----------------------------------------------------------------------------
| AtssmartSensorDriver: Reads temperatures from hard disks using S.M.A.R.T.  |
| via device files like /dev/sda.                                            |
----------------------------------------------------------------------------*/

AtasmartSensorDriver::AtasmartSensorDriver(
	string path,
	bool optional,
	opt<vector<int>> correction,
	opt<unsigned int> max_errors
)
: SensorDriver(path, optional, correction, max_errors)
{
	set_num_temps(1);
}

void AtasmartSensorDriver::init()
{
	if (sk_disk_open(path().c_str(), &disk_) < 0) {
		string msg = std::strerror(errno);
		throw SystemError("sk_disk_open(" + path() + "): " + msg);
	}
}


AtasmartSensorDriver::~AtasmartSensorDriver()
{ sk_disk_free(disk_); }


void AtasmartSensorDriver::read_temps_()
{
	SkBool disk_sleeping = false;

	if (unlikely(dnd_disk && (sk_disk_check_sleep_mode(disk_, &disk_sleeping) < 0))) {
		string msg = strerror(errno);
		throw SystemError("sk_disk_check_sleep_mode(" + path() + "): " + msg);
	}

	if (unlikely(disk_sleeping)) {
		temp_state_.add_temp(0);
	}
	else {
		uint64_t mKelvin;
		float tmp;

		if (unlikely(sk_disk_smart_read_data(disk_) < 0)) {
			string msg = strerror(errno);
			throw SystemError("sk_disk_smart_read_data(" + path() + "): " + msg);
		}
		if (unlikely(sk_disk_smart_get_temperature(disk_, &mKelvin)) < 0) {
			string msg = strerror(errno);
			throw SystemError("sk_disk_smart_get_temperature(" + path() + "): " + msg);
		}

		tmp = mKelvin / 1000.0f;
		tmp -= 273.15f;

		if (unlikely(tmp > std::floor(numeric_limits<int>::max()) || tmp < std::numeric_limits<int>::min())) {
			throw SystemError(MSG_T_GET(path()) + std::to_string(tmp) + " isn't a valid temperature.");
		}

		temp_state_.add_temp(int(tmp) + correction_[0]);
	}
}

string AtasmartSensorDriver::lookup()
{ return path(); }

#endif /* USE_ATASMART */


#ifdef USE_NVML
/*----------------------------------------------------------------------------
| NvmlSensorDriver: Gets temperatures directly from GPUs supported by the    |
| nVidia Management Library that is included with the proprietary driver.    |
----------------------------------------------------------------------------*/

NvmlSensorDriver::NvmlSensorDriver(string bus_id, bool optional, opt<vector<int>> correction, opt<unsigned int> max_errors)
: SensorDriver(bus_id, optional, correction, max_errors),
  dl_nvmlInit_v2(nullptr),
  dl_nvmlDeviceGetHandleByPciBusId_v2(nullptr),
  dl_nvmlDeviceGetName(nullptr),
  dl_nvmlDeviceGetTemperature(nullptr),
  dl_nvmlShutdown(nullptr)
{
	if (!(nvml_so_handle_ = dlopen("libnvidia-ml.so.1", RTLD_LAZY))) {
		string msg = strerror(errno);
		throw SystemError("Failed to load libnvidia-ml.so.1: " + msg);
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
	set_num_temps(1);
}


void NvmlSensorDriver::init()
{
	nvmlReturn_t ret;
	string name, brand;
	name.resize(256);
	brand.resize(256);

	if ((ret = dl_nvmlInit_v2()))
		throw SystemError("Failed to initialize NVML driver. Error code (cf. nvml.h): " + std::to_string(ret));
	if ((ret = dl_nvmlDeviceGetHandleByPciBusId_v2(path().c_str(), &device_)))
		throw SystemError("Failed to open PCI device " + path() + ". Error code (cf. nvml.h): " + std::to_string(ret));
	dl_nvmlDeviceGetName(device_, &*name.begin(), 255);
	log(TF_DBG) << "Initialized NVML sensor on " << name << " at PCI " << path() << "." << flush;
}


NvmlSensorDriver::~NvmlSensorDriver() noexcept(false)
{
	nvmlReturn_t ret;
	if ((ret = dl_nvmlShutdown()))
		log(TF_ERR) << "Failed to shutdown NVML driver. Error code (cf. nvml.h): " << std::to_string(ret);
	dlclose(nvml_so_handle_);
}


void NvmlSensorDriver::read_temps_()
{
	nvmlReturn_t ret;
	unsigned int tmp;
	if ((ret = dl_nvmlDeviceGetTemperature(device_, NVML_TEMPERATURE_GPU, &tmp)))
		throw SystemError(MSG_T_GET(path()) + "Error code (cf. nvml.h): " + std::to_string(ret));
	temp_state_.add_temp(int(tmp));
}

string NvmlSensorDriver::lookup()
{ return path(); }

#endif /* USE_NVML */


#ifdef USE_LM_SENSORS

/*----------------------------------------------------------------------------
| LMSensorsDriver: Driver for sensors provided by LM sensors's `libsensors`. |
----------------------------------------------------------------------------*/

// Closest integer value to zero Kelvin.
static const int MIN_CELSIUS_TEMP = -273;

std::once_flag LMSensorsDriver::lm_sensors_once_init_;


LMSensorsDriver::LMSensorsDriver(
	string chip_name,
	vector<string> feature_names,
	bool optional,
	opt<vector<int>> correction,
	opt<unsigned int> max_errors
)
: SensorDriver(chip_name, optional, correction, max_errors),
  chip_name_(chip_name),
  chip_(nullptr),
  feature_names_(feature_names)
{
	std::call_once(lm_sensors_once_init_, initialize_lm_sensors);
	set_num_temps(feature_names_.size());
}


void LMSensorsDriver::init()
{
	chip_ = LMSensorsDriver::find_chip_by_name(chip_name_);
	set_path(chip_->path);

	for (const string& feature_name : feature_names_) {
		auto feature = LMSensorsDriver::find_feature_by_name(*chip_, feature_name);
		if (!feature)
			throw SystemError("LM sensors chip '" + chip_name_
				+ "' does not have the feature '" + feature_name + "'");
		features_.push_back(feature);

		auto sub_feature = ::sensors_get_subfeature(chip_, feature, ::SENSORS_SUBFEATURE_TEMP_INPUT);
		if (!sub_feature)
			throw SystemError("LM sensors feature ID '" + feature_name
				+ "' of the chip '" + chip_name_
				+ "' does not have a temperature input sensor");
		sub_features_.push_back(sub_feature);

		log(TF_DBG) << "Initialized LM sensors temperature input of feature '"
			+ feature_name + "' of chip '" + chip_name_ + "'." << flush;
	}

	if (correction_.empty())
		correction_.resize(feature_names_.size(), 0);

}


LMSensorsDriver::~LMSensorsDriver()
{}


void LMSensorsDriver::initialize_lm_sensors()
{
	::sensors_parse_error = LMSensorsDriver::parse_error_callback;
	::sensors_parse_error_wfn = LMSensorsDriver::parse_error_wfn_callback;
	::sensors_fatal_error = LMSensorsDriver::fatal_error_callback;

	int err;
	if ((err = ::sensors_init(nullptr)))
		throw SystemError(string("Failed to initialize LM sensors driver: ") + sensors_strerror(err));

	atexit(::sensors_cleanup);
	log(TF_DBG) << "Initialized LM sensors." << flush;
}


const ::sensors_chip_name* LMSensorsDriver::find_chip_by_name(
	const string& chip_name
) {
	int state = 0;
	for (;;) {
		auto chip = ::sensors_get_detected_chips(nullptr, &state);
		if (!chip)
			break;

		if (chip_name == LMSensorsDriver::get_chip_name(*chip))
			return chip;
	}

	throw SystemError("LM sensors chip '" + chip_name + "' was not found");
}


string LMSensorsDriver::get_chip_name(const ::sensors_chip_name& chip)
{
	int len = sensors_snprintf_chip_name(nullptr, 0, &chip);
	if (len < 0) {
		const char *msg = ::sensors_strerror(len);
		throw SystemError(string("Failed to get LM sensors chip name: ") + msg);
	}

	vector<char> buffer(len + 1);
	int w_sz = sensors_snprintf_chip_name(buffer.data(), size_t(len + 1), &chip);
	if (w_sz < 0) {
		const char *msg = ::sensors_strerror(w_sz);
		throw SystemError(string("Failed to get LM sensors chip name: ") + msg);
	} else if (w_sz >= (len + 1)) {
		throw SystemError("LM sensors chip name is too long");
	}

	return string(buffer.data(), w_sz);
}


const ::sensors_feature* LMSensorsDriver::find_feature_by_name(
	const ::sensors_chip_name& chip,
	const string& feature_name
) {
	int state = 0;

	for (;;) {
		auto feature = ::sensors_get_features(&chip, &state);
		if (!feature)
			break;

		char *label = ::sensors_get_label(&chip, feature);
		bool label_matches = (feature_name == label);
		free(label);

		if (label_matches)
			return feature;
	}

	return nullptr;
}


void LMSensorsDriver::parse_error_callback(const char *err, int line_no)
{
	log(TF_ERR) << "LM sensors parsing error: " << err << " in line "
		<< std::to_string(line_no);
}


void LMSensorsDriver::parse_error_wfn_callback(const char *err, const char *file_name, int line_no)
{
	log(TF_ERR) << "LM sensors parsing error: " << err << " in file '"
		<< file_name << "' at line " << std::to_string(line_no);
}


void LMSensorsDriver::fatal_error_callback(const char *proc, const char *err)
{
	log(TF_ERR) << "LM sensors fatal error in " << proc << ": " << err;

	// libsensors documentation for sensors_fatal_error() requires this
	// function to never return.
	//
	// We can also consider calling abort() in order to generate a core dump
	// in addition to reporting failure.
	abort();
}


void LMSensorsDriver::read_temps_()
{
	size_t len = sub_features_.size();
	for (size_t index = 0; index < len; ++index) {
		auto sub_feature = sub_features_[index];

		double real_value = MIN_CELSIUS_TEMP;
		int err = ::sensors_get_value(chip_, sub_feature->number, &real_value);
		if (err) {
			const char *msg = ::sensors_strerror(err);
			throw SystemError(
				string("temperature input value of feature '")
				+ feature_names_[index] + "' of chip '" + chip_name_
				+ "' is unavailable: " + msg
			);
		}
		else {
			int integer_value;
			integer_value = int(real_value) + correction_[index];

			// Make sure the reported value is physically valid.
			if (integer_value < MIN_CELSIUS_TEMP)
				throw SystemError("Invalid temperature on feature '" + feature_names_[index]
					+ "' of chip '" + chip_name_ + "': " + std::to_string(integer_value)
				);
			else
				temp_state_.add_temp(integer_value);
		}
	}
}

string LMSensorsDriver::lookup()
{ return path(); }


#endif /* USE_LM_SENSORS */


}
