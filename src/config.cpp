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


Config::Config() : fan_(nullptr) {}


Config *Config::parse_config(string filename)
{
	ifstream f_in(filename);
	f_in.seekg(0, f_in.end);
	ifstream::pos_type f_size = f_in.tellg();
	f_in.seekg(0, f_in.beg);
	string f_data;
	f_data.resize(f_size, 0);
	f_in.read(&*f_data.begin(), f_size);

	string::size_type offset = 0;
	ConfigParser parser;
	Config *config = parser.parse(f_data, offset);
	return config;
}

Config::~Config()
{}

Level::Level(int level) : level_n_(level), level_s_(to_string(level)) {}
Level::Level(string level) : level_n_(stoi(level)), level_s_(level) {}


SimpleLevel::SimpleLevel(string level, long lower_limit, long upper_limit)
: Level(level), lower_limit_(lower_limit), upper_limit_(upper_limit)
{}

SimpleLevel::SimpleLevel(long level, long lower_limit, long upper_limit)
: Level(level), lower_limit_(lower_limit), upper_limit_(upper_limit)
{}


ComplexLevel::ComplexLevel(long level, vector<long> lower_limit, vector<long> upper_limit)
: Level(level), lower_limit_(lower_limit), upper_limit_(upper_limit)
{}

ComplexLevel::ComplexLevel(string level, vector<long> lower_limit, vector<long> upper_limit)
: Level(level), lower_limit_(lower_limit), upper_limit_(upper_limit)
{}




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

Fan::Fan(FanDriver *driver) : driver(driver) {}

TpFan::TpFan(string path) : Fan(new TpFanDriver(path)) {}

PwmFan::PwmFan(string path) : Fan(new HwmonFanDriver(path)) {}



} /* namespace thinkfan */
