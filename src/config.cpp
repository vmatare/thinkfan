/*
 * Config.cpp
 *
 *  Created on: 22.06.2014
 *      Author: ich
 */

#include "config.h"
#include <iterator>
#include <stdexcept>
#include <iostream>

namespace thinkfan {

using namespace std;


Config::Config(string filename)
: fan_(nullptr),
  parser_comment("^[ \t]*#.*$", 0),
  parser_fan("fan"),
  parser_tp_fan("tp_fan"),
  parser_pwm_fan("pwm_fan"),
  parser_sensor("sensor"),
  parser_hwmon("hwmon"),
  parser_tp_thermal("tp_thermal")
{
	ifstream f_in(filename);
	f_in.seekg(0, f_in.end);
	ifstream::pos_type f_size = f_in.tellg();
	f_in.seekg(0, f_in.beg);
	string f_data;
	f_data.resize(f_size, 0);
	f_in.read(&*f_data.begin(), f_size);

	string *match;
	string::size_type offset = 0;
	while((match = parser_comment.parse(f_data, offset))) {
	}
}

Config::~Config()
{}


Level::Level(int level, vector<long> lower_limit, vector<long> upper_limit)
: level_n_(level),
  level_s_(to_string(level)),
  lower_limit_(lower_limit),
  upper_limit_(upper_limit)
{}

Level::Level(string level, vector<long> lower_limit, vector<long> upper_limit)
: level_n_(stoi(level)),
  level_s_(level),
  lower_limit_(lower_limit),
  upper_limit_(upper_limit)
{}

Level::Level(string *level)
: level_s_(*level),
  level_n_(stoi(level_s_)),
  lower_limit_(),
  upper_limit_()
{

}

const string Fan::tpacpi_path = "/proc/acpi/ibm";
Fan::Fan(string path)
{
	if (path.substr(0, tpacpi_path.length()) == tpacpi_path) {
		this->driver = new TpFanDriver(path);
	}
	else {
		this->driver = new HwmonFanDriver(path);
	}
}

Fan::~Fan() {
}



} /* namespace thinkfan */
