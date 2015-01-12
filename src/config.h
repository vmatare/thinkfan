/*
 * Config.h
 *
 *  Created on: 22.06.2014
 *      Author: ich
 */

#ifndef THINKFAN_CONFIG_H_
#define THINKFAN_CONFIG_H_

#include <list>
#include <string>
#include <regex.h>

namespace thinkfan {
class Fan;
class Level;
}

#include "drivers.h"
#include "parser.h"

namespace thinkfan {

using namespace std;

class Config {
private:
	Fan *fan_;
	const RegexParser parser_comment;

	const KeywordParser parser_fan;
	const KeywordParser parser_tp_fan;
	const KeywordParser parser_pwm_fan;

	const KeywordParser parser_sensor;
	const KeywordParser parser_hwmon;
	const KeywordParser parser_tp_thermal;

public:
	Config(string filename);
	virtual ~Config();
};


class Level {
private:
	int level_n_;
	string level_s_;
public:
	Level(string *level);
};


class SimpleLevel {
};


class ComplexLevel {
private:
	vector<long> lower_limit_;
	vector<long> upper_limit_;
public:
	ComplexLevel(int level, vector<long> lower_limit, vector<long> upper_limit);
	ComplexLevel(string level, vector<long> lower_limit, vector<long> upper_limit);
};


class Fan {
private:
	static const string tpacpi_path;
	FanDriver *driver;
public:
	Fan(std::string path);
	virtual ~Fan();
};


class TpFan : public Fan {
public:
	TpFan(std::string path);
};




}

#endif /* THINKFAN_CONFIG_H_ */
