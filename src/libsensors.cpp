/********************************************************************
 * libsensors.cpp: State management for libsensors
 * (C) 2022, Victor Mataré
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

#include "libsensors.h"
#include "error.h"
#include "sensors.h"
#include "message.h"

namespace thinkfan {

std::weak_ptr<LibsensorsInterface> LibsensorsInterface::instance_;


LibsensorsInterface::LibsensorsInterface()
: libsensors_initialized_(false)
{
	::sensors_parse_error = parse_error_callback;
	::sensors_parse_error_wfn = parse_error_wfn_callback;
	::sensors_fatal_error = fatal_error_callback;

	log(TF_DBG) << "Initialized LM sensors." << flush;
}

LibsensorsInterface::~LibsensorsInterface()
{
	if (libsensors_initialized_)
		::sensors_cleanup();
}


LibsensorsInterface::InitGuard::InitGuard(LMSensorsDriver *client)
: client_(client)
, iface_(LibsensorsInterface::instance_.lock())
{
	if (!iface_->libsensors_initialized_) {
		int err;
		if ((err = ::sensors_init(nullptr)))
			throw SystemError(string("Failed to initialize LM sensors driver: ") + sensors_strerror(err));
		iface_->libsensors_initialized_ = true;
	}
}


LibsensorsInterface::InitGuard::~InitGuard()
{
	if (iface_->libsensors_initialized_ && iface_->clients_[client_].features.empty()) {

		// Make all clients unavailable (they have to lookup again!)
		for (auto drv_entry : iface_->clients_)
			drv_entry.first->set_unavailable();
		iface_->clients_.clear();

		::sensors_cleanup();
		iface_->libsensors_initialized_ = false;
	}
}


shared_ptr<LibsensorsInterface> LibsensorsInterface::instance()
{
	shared_ptr<LibsensorsInterface> rv;
	if (instance_.expired()) {
		rv.reset(new LibsensorsInterface());
		instance_ = rv;
	}
	else
		rv = instance_.lock();

	return rv;
}


string LibsensorsInterface::lookup_client_features(LMSensorsDriver *client)
{
	chip_features cf;
	InitGuard ig(client);

	cf.chip = find_chip_by_name(client->chip_name());

	for (const string& feature_name : client->feature_names()) {
		auto feature = find_feature_by_name(*cf.chip, feature_name);
		if (!feature)
			throw SystemError("LM sensors chip '" + client->chip_name()
				+ "' does not have the feature '" + feature_name + "'");

		auto sub_feature = ::sensors_get_subfeature(cf.chip, feature, ::SENSORS_SUBFEATURE_TEMP_INPUT);
		if (!sub_feature)
			throw SystemError("LM sensors feature ID '" + feature_name
				+ "' of the chip '" + client->chip_name()
				+ "' does not have a temperature input sensor");
		cf.features.push_back({feature, sub_feature});

		log(TF_DBG) << "Initialized LM sensors temperature input of feature '"
			+ feature_name + "' of chip '" + client->chip_name() + "'." << flush;
	}

	clients_.insert({client, cf});
	return cf.chip->path;
}


vector<double> LibsensorsInterface::get_temps(LMSensorsDriver *client)
{
	chip_features &cf = clients_.at(client);
	vector<double> rv;

	for (auto chip_feature : cf.features) {
		auto sub_feature = chip_feature.second;
		double real_value = MIN_CELSIUS_TEMP;

		int err = ::sensors_get_value(cf.chip, sub_feature->number, &real_value);
		if (err)
			throw SystemError(
				string("temperature input value of feature '") + chip_feature.first->name
				+ "' of chip '" + client->chip_name()
				+ "' is unavailable: " + ::sensors_strerror(err)
			);
		else if (real_value < MIN_CELSIUS_TEMP) // Make sure the reported value is physically valid.
			throw SystemError(
				string("Invalid temperature on feature '") + chip_feature.first->name
				+ "' of chip '" + client->chip_name()
				+ "': " + std::to_string(real_value)
			);

		rv.push_back(real_value);
	}

	return rv;
}


const ::sensors_chip_name* LibsensorsInterface::find_chip_by_name(
	const string& chip_name
) {
	int state = 0;
	for (;;) {
		auto chip = ::sensors_get_detected_chips(nullptr, &state);
		if (!chip)
			break;

		if (chip_name == get_chip_name(*chip))
			return chip;
	}

	throw SystemError("LM sensors chip '" + chip_name + "' was not found");
}


string LibsensorsInterface::get_chip_name(const ::sensors_chip_name& chip)
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


const ::sensors_feature* LibsensorsInterface::find_feature_by_name(
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


void LibsensorsInterface::parse_error_callback(const char *err, int line_no)
{
	log(TF_ERR) << "LM sensors parsing error: " << err << " in line "
		<< std::to_string(line_no);
}


void LibsensorsInterface::parse_error_wfn_callback(const char *err, const char *file_name, int line_no)
{
	log(TF_ERR) << "LM sensors parsing error: " << err << " in file '"
		<< file_name << "' at line " << std::to_string(line_no);
}


void LibsensorsInterface::fatal_error_callback(const char *proc, const char *err)
{
	log(TF_ERR) << "LM sensors fatal error in " << proc << ": " << err;

	// libsensors documentation for sensors_fatal_error() requires this
	// function to never return.
	//
	// We call abort() in order to generate a core dump in addition to reporting failure.
	abort();
}

}
