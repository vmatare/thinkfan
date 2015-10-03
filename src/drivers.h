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

#ifndef THINKFAN_DRIVERS_H_
#define THINKFAN_DRIVERS_H_

#include <string>
#include "thinkfan.h"

namespace thinkfan {

class Level;

class FanDriver {
protected:
	string path_;
	string initial_state_;
	std::chrono::duration<unsigned int> watchdog_;
	std::chrono::duration<float> depulse_;
	std::chrono::system_clock::time_point last_watchdog_ping_;
	FanDriver(const string &path, const unsigned int watchdog_timeout = 120);
public:
	FanDriver() : watchdog_(0) {}
	bool is_default() { return path_.length() == 0; }
	virtual ~FanDriver() = default;
	virtual void set_speed(const string &level);
	virtual void init() const {}
	virtual void ping_watchdog_and_depulse(const string &level) {};
};


class TpFanDriver : public FanDriver {
public:
	TpFanDriver(const string &path);
	~TpFanDriver() override;
	void set_watchdog(const unsigned int timeout);
	void set_depulse(float duration);
	void init() const override;
	void set_speed(const string &level);
	virtual void ping_watchdog_and_depulse(const string &level) override;
};


class HwmonFanDriver : public FanDriver {
public:
	HwmonFanDriver(const string &path);
	void init() const override;
};


class SensorDriver {
protected:
	string path_;
	unsigned int num_temps_;
	SensorDriver(string path);
	std::vector<int> correction_;
public:
	virtual ~SensorDriver() = default;
	virtual void read_temps() const = 0;
	unsigned int num_temps() const { return num_temps_; }
	void set_correction(const std::vector<int> &correction);
};


class TpSensorDriver : public SensorDriver {
public:
	TpSensorDriver(string path);
	void read_temps() const override;
};


class HwmonSensorDriver : public SensorDriver {
public:
	HwmonSensorDriver(string path);
	void read_temps() const override;
};


}

#endif /* THINKFAN_DRIVERS_H_ */
