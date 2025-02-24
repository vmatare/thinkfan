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
#include "driver.h"
#include "hwmon.h"

namespace thinkfan {

class Level;

class FanDriver : public Driver {
protected:
	FanDriver(bool optional, const unsigned int watchdog_timeout = 120, opt<unsigned int> max_errors = nullopt);

public:
	bool is_default() { return path().length() == 0; }
	virtual ~FanDriver() noexcept(false) override;
	virtual void set_speed(const Level &level) = 0;
	const string &current_speed() const;
	virtual void ping_watchdog_and_depulse(const Level &) {}
	bool operator == (const FanDriver &other) const;

protected:
	void set_speed(const string &level);

	string initial_state_;
	string current_speed_;
	seconds watchdog_;
	secondsf depulse_;
	std::chrono::system_clock::time_point last_watchdog_ping_;

private:
	virtual void skip_io_error(const ExpectedError &e) override;
	void set_speed_(const string &level);
};


class TpFanDriver : public FanDriver {
public:
	TpFanDriver(const string &path, bool optional = false, opt<unsigned int> max_errors = nullopt);

	virtual ~TpFanDriver() noexcept(false) override;
	void set_watchdog(const unsigned int timeout);
	void set_depulse(float duration);
	virtual void set_speed(const Level &level) override;
	virtual void ping_watchdog_and_depulse(const Level &level) override;

protected:
	virtual void init() override;
	virtual string lookup() override;
	virtual string type_name() const override;

private:
	const string path_;
};


class HwmonFanDriver : public FanDriver {
public:
	HwmonFanDriver(const string &path);

	HwmonFanDriver(
		shared_ptr<HwmonInterface<FanDriver>> hwmon_interface,
		bool optional,
		opt<unsigned int> max_errors = nullopt,
		bool skip_save = false
	);

	virtual ~HwmonFanDriver() noexcept(false) override;
	virtual void set_speed(const Level &level) override;

protected:
	virtual void init() override;
	virtual string lookup() override;
	virtual string type_name() const override;

private:
	shared_ptr<HwmonInterface<FanDriver>> hwmon_interface_;
	bool skip_save_;
};


}
