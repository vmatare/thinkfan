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

#ifndef THINKFAN_CONFIG_H_
#define THINKFAN_CONFIG_H_

#include "error.h"

#include <string>
#include <vector>
#include <memory>
#include <map>

#include "drivers.h"
#include "thinkfan.h"

namespace thinkfan {



class FanConfig {
public:
	FanConfig();
	virtual ~FanConfig() = default;
	virtual void set_fanspeed(const TemperatureState &) = 0;
	virtual void ensure_consistency() = 0;
	bool set_fan(std::unique_ptr<FanDriver> &&);
	const std::unique_ptr<FanDriver> &fan() const;

private:
	std::unique_ptr<FanDriver> fan_;
};


class StepwiseMapping : public FanConfig {
public:
	StepwiseMapping();
	virtual ~StepwiseMapping() override = default;
	virtual void set_fanspeed(const TemperatureState &) override;
	virtual void ensure_consistency() override;
	bool add_level(std::unique_ptr<Level> &&level);
	const std::vector<std::unique_ptr<Level>> &levels() const;

private:
	std::vector<std::unique_ptr<Level>> levels_;
	std::vector<std::unique_ptr<Level>>::iterator cur_lvl_;
};


class Level {
protected:
	string level_s_;
	int level_n_;
	std::vector<int> lower_limit_;
	std::vector<int> upper_limit_;
public:
	Level(int level, int lower_limit, int upper_limit);
	Level(string level, int lower_limit, int upper_limit);
	Level(int level, const std::vector<int> &lower_limit, const std::vector<int> &upper_limit);
	Level(string level, const std::vector<int> &lower_limit, const std::vector<int> &upper_limit);

	virtual ~Level() = default;

	const std::vector<int> &lower_limit() const;
	const std::vector<int> &upper_limit() const;

	virtual bool up() const = 0;
	virtual bool down() const = 0;

	const string &str() const;
	int num() const;

	static int string_to_int(string &level);
};



class SimpleLevel : public Level {
public:
	SimpleLevel(int level, int lower_limit, int upper_limit);
	SimpleLevel(string level, int lower_limit, int upper_limit);
	virtual bool up() const override;
	virtual bool down() const override;
};


class ComplexLevel : public Level {
public:
	ComplexLevel(int level, const std::vector<int> &lower_limit, const std::vector<int> &upper_limit);
	ComplexLevel(string level, const std::vector<int> &lower_limit, const std::vector<int> &upper_limit);
	virtual bool up() const override;
	virtual bool down() const override;
};


class Config {
public:
	Config();
	// Not trivially copyable since it holds raw pointers to levels, drivers and fans.
	Config(const Config &) = delete;
	~Config() = default;
	static const Config *read_config(const std::vector<string> &filenames);
	bool add_sensor(std::unique_ptr<SensorDriver> &&sensor);
	bool add_fan_config(std::unique_ptr<FanConfig> &&fan_cfg);
	void ensure_consistency() const;
	void init_fans() const;

	unsigned int num_temps() const;
	const std::vector<std::unique_ptr<SensorDriver>> &sensors() const;
	const std::vector<std::unique_ptr<FanConfig>> &fan_configs() const;

	// No copy assignment operator either (not required).
	Config &operator = (const Config &) = delete;

	string src_file;
private:
	static const Config *try_read_config(const string &data);
	std::vector<std::unique_ptr<SensorDriver>> sensors_;
	std::vector<std::unique_ptr<FanConfig>> temp_mappings_;
	unsigned int num_temps_;
};


}

#endif /* THINKFAN_CONFIG_H_ */
