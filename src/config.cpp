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
#include <algorithm>
#include <fstream>
#include <limits>
#include <cstring>
#include <cerrno>
#include "parser.h"
#include "message.h"
#include "thinkfan.h"

#ifdef USE_YAML
#include "yamlconfig.h"
#endif

namespace thinkfan {


FanConfig::FanConfig(std::unique_ptr<FanDriver> &&fan_drv)
: fan_(std::move(fan_drv))
{}

const unique_ptr<FanDriver> &FanConfig::fan() const
{ return fan_; }

void FanConfig::set_fan(unique_ptr<FanDriver> &&fan)
{ fan_ = std::move(fan); }



StepwiseMapping::StepwiseMapping(std::unique_ptr<FanDriver> &&fan_drv)
: FanConfig(std::move(fan_drv))
{}

const std::vector<unique_ptr<Level>> &StepwiseMapping::levels() const
{ return levels_; }

void StepwiseMapping::init_fanspeed(const TemperatureState &)
{
	cur_lvl_ = --levels().end();
	while (cur_lvl_ != levels().begin() && (*cur_lvl_)->down())
		cur_lvl_--;
	fan()->set_speed(**cur_lvl_);
}

bool StepwiseMapping::set_fanspeed(const TemperatureState &temp_state)
{
	if (unlikely(cur_lvl_ != --levels().end() && (*cur_lvl_)->up())) {
		while (cur_lvl_ != --levels().end() && (*cur_lvl_)->up())
			cur_lvl_++;
		fan()->set_speed(**cur_lvl_);
		return true;
	}
	else if (unlikely(cur_lvl_ != levels().begin() && (*cur_lvl_)->down())) {
		while (cur_lvl_ != levels().begin() && (*cur_lvl_)->down())
			cur_lvl_--;
		fan()->set_speed(**cur_lvl_);
		tmp_sleeptime = sleeptime;
		return true;
	}
	else {
		fan()->ping_watchdog_and_depulse(**cur_lvl_);
		return false;
	}
}


void StepwiseMapping::ensure_consistency()
{
	if (levels().size() == 0)
		throw ConfigError("No fan levels specified.");

	if (!fan())
		throw ConfigError("No fan specified in stepwise mapping.");

	int maxlvl = (*levels_.rbegin())->num();
	if (dynamic_cast<const HwmonFanDriver *>(fan().get()) && maxlvl < 128)
		error<ConfigError>(MSG_CONF_MAXLVL((*levels_.rbegin())->num()));
	else if (dynamic_cast<const TpFanDriver *>(fan().get())
			 && maxlvl != std::numeric_limits<int>::max()
			 && maxlvl > 7
			 && maxlvl != 127)
		error<ConfigError>(MSG_CONF_TP_LVL7(maxlvl, 7));

}


void StepwiseMapping::add_level(std::unique_ptr<Level> &&level)
{
	if (levels_.size() > 0) {
		const unique_ptr<Level> &last_lvl = levels_.back();
		if (level->num() != std::numeric_limits<int>::max()
				&& level->num() != std::numeric_limits<int>::min()
				&& last_lvl->num() > level->num())
			error<ConfigError>(MSG_CONF_LVLORDER);

		if (last_lvl->upper_limit().size() != level->upper_limit().size())
			error<ConfigError>(MSG_CONF_LVLORDER);

		for (std::vector<int>::const_iterator mit = last_lvl->upper_limit().begin(), oit = level->lower_limit().begin();
				mit != last_lvl->upper_limit().end() && oit != level->lower_limit().end();
				++mit, ++oit)
		{
			if (*mit < *oit) error<ConfigError>(MSG_CONF_OVERLAP);
		}
	}

	levels_.push_back(std::move(level));
}




Config::Config()
: num_temps_(0)
{}


const Config *Config::read_config(const std::vector<string> &filenames)
{
	const Config *rv = nullptr;
	for (auto it = filenames.begin(); it != filenames.end(); ++it) {
		try {
			rv = try_read_config(*it);
			break;
		} catch (IOerror &e) {
			if (e.code() != ENOENT || it+1 >= filenames.end())
				throw;
		}
	}

	return rv;
}


const Config *Config::try_read_config(const string &filename)
{
	Config *rv = nullptr;

	ifstream f_in(filename);
	if (!(f_in.is_open() && f_in.good()))
		throw IOerror(filename + ": ", errno);
	if (!f_in.seekg(0, f_in.end))
		throw IOerror(filename + ": ", errno);
	ifstream::pos_type f_size = f_in.tellg();
	if (!f_in.seekg(0, f_in.beg))
		throw IOerror(filename + ": ", errno);
	string f_data;
	f_data.resize(f_size, 0);
	if (!f_in.read(&*f_data.begin(), f_size))
		throw IOerror(filename + ": ", errno);

#ifdef USE_YAML
	try	{
		YAML::Node root = YAML::Load(f_data);

		// Copy the return value first. Workaround for https://github.com/vmatare/thinkfan/issues/42
		// due to bug in ancient yaml-cpp: https://github.com/jbeder/yaml-cpp/commit/97d56c3f3608331baaee26e17d2f116d799a7edc
		try {
			YAML::wtf_ptr<Config> rv_tmp = root.as<YAML::wtf_ptr<Config>>();
			rv = rv_tmp.release();
		} catch (IOerror &e) {
			// An IOerror while processing the config means that an invalid sensor or fan path was specified.
			// That's a user error, wrap it and let it escalate.
			throw ConfigError(e.what());
		}
#if not defined(DISABLE_EXCEPTION_CATCHING)
	} catch(YamlError &e) {
		throw ConfigError(filename, e.mark, f_data, e.what());
	} catch(YAML::RepresentationException &e) {
		throw ConfigError(filename, e.mark, f_data, "Invalid entry");
#endif
	} catch(YAML::ParserException &e) {
		string::size_type ext_off = filename.rfind('.');
		if (ext_off != string::npos) {
			string ext = filename.substr(filename.rfind('.'));
			std::for_each(ext.begin(), ext.end(), [] (char &c) {
				c = std::toupper(c, std::locale());
			} );
			if (ext == ".YAML")
				throw ConfigError(filename, e.mark, f_data, e.what());
		}
#endif //USE_YAML

		ConfigParser parser;

		const char *input = f_data.c_str();
		const char *start = input;

		rv = parser.parse_config(input);

		if (!rv) {
			throw SyntaxError(filename, parser.get_max_addr() - start, f_data);
		}
#ifdef USE_YAML
	}
#endif //USE_YAML

	rv->src_file = filename;
	rv->ensure_consistency();

	return rv;
}


void Config::ensure_consistency() const
{
	// Consistency checks which require the complete config

	for (const std::unique_ptr<FanConfig> &fan_cfg : fan_configs())
		try {
			fan_cfg->ensure_consistency();
		} catch (ConfigError &err) {
			err.set_filename(src_file);
			throw;
		}

	if (sensors().size() < 1)
		throw ConfigError(src_file + ": " + MSG_NO_SENSOR);
}



void Config::add_sensor(std::unique_ptr<SensorDriver> &&sensor)
{
	num_temps_ += sensor->num_temps();
	sensors_.push_back(std::move(sensor));
}

unsigned int Config::num_temps() const
{ return num_temps_; }

const std::vector<unique_ptr<SensorDriver>> &Config::sensors() const
{ return sensors_; }

const std::vector<std::unique_ptr<FanConfig>> &Config::fan_configs() const
{ return temp_mappings_; }

void Config::add_fan_config(std::unique_ptr<FanConfig> &&fan_cfg)
{ temp_mappings_.push_back(std::move(fan_cfg)); }


void Config::init_fans() const
{
	for (const std::unique_ptr<FanConfig> &fan_cfg : fan_configs())
		fan_cfg->fan()->init();
}



Level::Level(int level, int lower_limit, int upper_limit)
: Level(level, std::vector<int>(1, lower_limit), std::vector<int>(1, upper_limit))
{}

Level::Level(string level, int lower_limit, int upper_limit)
: Level(level, std::vector<int>(1, lower_limit), std::vector<int>(1, upper_limit))
{}

Level::Level(int level, const std::vector<int> &lower_limit, const std::vector<int> &upper_limit)
: level_s_("level " + std::to_string(level)),
  level_n_(level),
  lower_limit_(lower_limit),
  upper_limit_(upper_limit)
{}

Level::Level(string level, const std::vector<int> &lower_limit, const std::vector<int> &upper_limit)
: level_s_(level),
  level_n_(string_to_int(level_s_)),
  lower_limit_(lower_limit),
  upper_limit_(upper_limit)
{
	if (lower_limit.size() != upper_limit.size())
		error<ConfigError>(MSG_CONF_LIMITLEN);

	for (std::vector<int>::const_iterator l_it = lower_limit.begin(), u_it = upper_limit.begin();
			l_it != lower_limit.end() && u_it != upper_limit.end();
			++u_it, ++l_it) {
		if (*l_it != numeric_limits<int>::max() && *l_it >= *u_it)
			error<ConfigError>(MSG_CONF_LOWHIGH);
	}
}

int Level::string_to_int(string &level) {
	int rv;
	if (level == "level auto" || level == "level disengaged" || level == "level full-speed")
		return std::numeric_limits<int>::min();
	else if (sscanf(level.c_str(), "level %d", &rv) == 1)
		return rv;
	else try {
		rv = std::stoi(level);
		level = "level " + level;
	} catch (std::out_of_range &) {
		error<ConfigError>(MSG_CONF_LVLFORMAT(level));
	} catch (std::invalid_argument &) {
		error<ConfigError>(MSG_CONF_LVLFORMAT(level));
	}
	return rv;
}

const std::vector<int> &Level::lower_limit() const
{ return lower_limit_; }

const std::vector<int> &Level::upper_limit() const
{ return upper_limit_; }

const string &Level::str() const
{ return this->level_s_; }

int Level::num() const
{ return this->level_n_; }



SimpleLevel::SimpleLevel(int level, int lower_limit, int upper_limit)
: Level(level, lower_limit, upper_limit)
{}

SimpleLevel::SimpleLevel(string level, int lower_limit, int upper_limit)
: Level(level, lower_limit, upper_limit)
{}

bool SimpleLevel::up() const
{ return *temp_state.tmax >= upper_limit().front(); }

bool SimpleLevel::down() const
{ return *temp_state.tmax < lower_limit().front(); }



ComplexLevel::ComplexLevel(int level, const std::vector<int> &lower_limit, const std::vector<int> &upper_limit)
: Level(level, lower_limit, upper_limit)
{}


ComplexLevel::ComplexLevel(string level, const std::vector<int> &lower_limit, const std::vector<int> &upper_limit)
: Level(level, lower_limit, upper_limit)
{}


bool ComplexLevel::up() const
{
	std::vector<int>::const_iterator temp_it = temp_state.biased_temps().begin();
	auto upper_it = upper_limit().begin();

	while (temp_it != temp_state.biased_temps().end())
		if (*temp_it++ >= *upper_it++) return true;

	return false;
}


bool ComplexLevel::down() const
{
	auto temp_it = temp_state.biased_temps().begin();
	auto lower_it = lower_limit().begin();

	while (temp_it != temp_state.biased_temps().end() && *temp_it < *lower_it) {
		temp_it++;
		lower_it++;
	}

	return temp_it == temp_state.biased_temps().end();
}


} /* namespace thinkfan */
