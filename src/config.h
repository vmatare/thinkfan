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

#include "drivers.h"
#include "thinkfan.h"

namespace thinkfan {



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
	Config(const Config &) = delete;
	~Config();
	static const Config *read_config(const string &filename);
	bool add_fan(std::unique_ptr<FanDriver> &&fan);
	bool add_sensor(std::unique_ptr<const SensorDriver> &&sensor);
	bool add_level(std::unique_ptr<const Level> &&level);

	unsigned int num_temps() const;
	FanDriver *fan() const;
	const std::vector<const Level *> &levels() const;
	const std::vector<const SensorDriver *> &sensors() const;

	Config &operator = (const Config &) = delete;

private:
	std::vector<const SensorDriver *> sensors_;
	std::vector<const Level *> levels_;
	unsigned int num_temps_;
	FanDriver *fan_;
};


}

#endif /* THINKFAN_CONFIG_H_ */
