/********************************************************************
 * config.cpp: Config data structures and consistency checking.
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

#include "config.h"
#include <stdexcept>
#include <fstream>
#include <limits>
#include "parser.h"
#include "message.h"
#include "thinkfan.h"


namespace thinkfan {

Config::Config() : num_temps_(0), fan_(nullptr) {}


const Config *Config::read_config(const string &filename)
{
	ifstream f_in(filename);
	f_in.seekg(0, f_in.end);
	ifstream::pos_type f_size = f_in.tellg();
	f_in.seekg(0, f_in.beg);
	string f_data;
	f_data.resize(f_size, 0);
	f_in.read(&*f_data.begin(), f_size);

	ConfigParser parser;
	const char *input = f_data.c_str();
	const char *start = input;

	Config *rv = parser.parse_config(input);

	if (!rv) {
		fail(TF_ERR) << SyntaxError(filename, parser.get_max_addr() - start, f_data) << flush;
	}
	else {
		if (rv->levels().size() == 0) throw ConfigError("No fan levels specified.");

		if (!rv->fan()) {
			log(TF_WRN, TF_WRN) << MSG_CONF_DEFAULT_FAN << flush;
			rv->add_fan(unique_ptr<TpFanDriver>(new TpFanDriver(DEFAULT_FAN)));
		}

		if (rv->sensors().size() < 1) {
			log(TF_WRN, TF_WRN) << MSG_SENSOR_DEFAULT << flush;
			rv->add_sensor(unique_ptr<TpSensorDriver>(new TpSensorDriver(DEFAULT_SENSOR)));
		}
	}
	return rv;
}


Config::~Config()
{
	delete fan_;
	for (const SensorDriver *sensor : sensors_) delete sensor;
	for (const Level *level : levels_) delete level;
}


bool Config::add_fan(std::unique_ptr<FanDriver> &&fan)
{
	if (!fan) return false;

	if (fan_)
		fail(TF_WRN) << ConfigError(MSG_CONF_FAN) << flush;
	this->fan_ = fan.release();
	return true;
}


bool Config::add_sensor(std::unique_ptr<const SensorDriver> &&sensor)
{
	if (!sensor) return false;

	num_temps_ += sensor->num_temps();
	sensors_.push_back(sensor.release());
	return true;
}


bool Config::add_level(std::unique_ptr<const Level> &&level)
{
	if (!level) return false;

	if (levels_.size() > 0) {
		const Level *last_lvl = levels_.back();
		if (level->num() != std::numeric_limits<int>::max()
				&& level->num() != std::numeric_limits<int>::min()
				&& last_lvl->num() >= level->num())
			fail(TF_WRN) << ConfigError(MSG_CONF_LVLORDER) << flush;

		if (last_lvl->upper_limit().size() != level->upper_limit().size())
			fail(TF_WRN) << ConfigError(MSG_CONF_LIMITLEN) << flush;

		for (std::vector<int>::const_iterator mit = last_lvl->upper_limit().begin(), oit = level->lower_limit().begin();
				mit != last_lvl->upper_limit().end() && oit != level->lower_limit().end();
				++mit, ++oit)
		{
			if (*mit < *oit) fail(TF_WRN) << ConfigError(MSG_CONF_OVERLAP) << flush;
		}
	}

	levels_.push_back(level.release());
	return true;
}


FanDriver *Config::fan() const
{ return fan_; }

unsigned int Config::num_temps() const
{ return num_temps_; }

const std::vector<const Level *> &Config::levels() const
{ return levels_; }

const std::vector<const SensorDriver *> &Config::sensors() const
{ return sensors_; }


Level::Level(int level, int lower_limit, int upper_limit)
: Level(level, std::vector<int>(), std::vector<int>())
{
	lower_limit_.push_back(lower_limit);
	upper_limit_.push_back(upper_limit);
}


Level::Level(string level, int lower_limit, int upper_limit)
: Level(level, std::vector<int>(lower_limit), std::vector<int>(upper_limit))
{}


Level::Level(int level, const std::vector<int> &lower_limit, const std::vector<int> &upper_limit)
: level_s_("level " + std::to_string(level)),
  level_n_(level),
  lower_limit_(lower_limit),
  upper_limit_(upper_limit)
{}


Level::Level(string level, const std::vector<int> &lower_limit, const std::vector<int> &upper_limit)
: level_s_(level),
  level_n_(std::numeric_limits<int>::max()),
  lower_limit_(lower_limit),
  upper_limit_(upper_limit)
{
	if (lower_limit.size() != upper_limit.size())
		fail(TF_WRN) << ConfigError(MSG_CONF_LIMITLEN) << flush;

	for (std::vector<int>::const_iterator l_it = lower_limit.begin(), u_it = upper_limit.begin();
			l_it != lower_limit.end() && u_it != upper_limit.end();
			++u_it, ++l_it) {
		if (*l_it >= *u_it) fail(TF_WRN) << ConfigError(MSG_CONF_LOWHIGH) << flush;
	}

	if (level == "level auto" || level == "level disengaged")
		level_n_ = std::numeric_limits<int>::min();
	else if (sscanf(level.c_str(), "level %d", &level_n_) == 1)
		return;
	else try {
		level_n_ = std::stoi(level);
		level_s_ = "level " + level;
	} catch (std::out_of_range &e) {
		fail(TF_WRN) << ConfigError(MSG_CONF_LVLFORMAT(level)) << flush;
	} catch (std::invalid_argument &e) {
		fail(TF_WRN) << ConfigError(MSG_CONF_LVLFORMAT(level)) << flush;
	}
}


const std::vector<int> &Level::lower_limit() const
{ return lower_limit_; }

const std::vector<int> &Level::upper_limit() const
{ return upper_limit_; }

const string &Level::str() const
{ return this->level_s_; }

int Level::num() const
{ return this->level_n_; }


bool Level::operator <= (const TemperatureState &temps) const
{ throw Error("severe bug or compiler error, this function should never be called."); }


bool Level::operator > (const TemperatureState &temps) const
{ throw Error("severe bug or compiler error, this function should never be called."); }


SimpleLevel::SimpleLevel(int level, int lower_limit, int upper_limit)
: Level(level, lower_limit, upper_limit) {}

SimpleLevel::SimpleLevel(string level, int lower_limit, int upper_limit)
: Level(level, lower_limit, upper_limit) {}

bool SimpleLevel::operator <= (const TemperatureState &temp_state) const
{ return *temp_state.b_tmax >= upper_limit().front(); }


bool SimpleLevel::operator > (const TemperatureState &temp_state) const
{ return *temp_state.b_tmax < lower_limit().front(); }



ComplexLevel::ComplexLevel(int level, const std::vector<int> &lower_limit, const std::vector<int> &upper_limit)
: Level(level, lower_limit, upper_limit) {}

ComplexLevel::ComplexLevel(string level, const std::vector<int> &lower_limit, const std::vector<int> &upper_limit)
: Level(level, lower_limit, upper_limit) {}


bool ComplexLevel::operator <= (const TemperatureState &temp_state) const
{
	const int *temp_idx = temp_state.temps.data();
	const int *upper_idx = upper_limit().data();

	while (temp_idx <= &temp_state.temps.back())
		if (*temp_idx++ >= *upper_idx++) return true;

	return false;
}


bool ComplexLevel::operator > (const TemperatureState &temp_state) const
{
	const int *temp_idx = temp_state.temps.data();
	const int *lower_idx = lower_limit().data();

	while (temp_idx <= &temp_state.temps.back() && *temp_idx++ < *lower_idx++)
		;

	return temp_idx - 1 == &temp_state.temps.back();
}


} /* namespace thinkfan */
