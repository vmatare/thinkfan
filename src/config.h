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

public:
	Config();
	static Config *parse_config(string filename);
	virtual ~Config();
};


class SimpleConfig : public Config {
private:
	vector<SimpleLevel> levels;
public:
};


class Level {
private:
	int level_n_;
	string level_s_;
protected:
	Level(int level);
	Level(string level);
public:

};


class SimpleLevel : public Level {
private:
	long lower_limit_;
	long upper_limit_;
public:
	SimpleLevel(string level, long lower_limit, long upper_limit);
	SimpleLevel(long level, long lower_limit, long upper_limit);
};


class ComplexLevel : public Level {
private:
	vector<long> lower_limit_;
	vector<long> upper_limit_;
public:
	ComplexLevel(long level, vector<long> lower_limit, vector<long> upper_limit);
	ComplexLevel(string level, vector<long> lower_limit, vector<long> upper_limit);
};


class Fan {
private:
	static const string tpacpi_path;
	FanDriver *driver;
public:
	Fan(std::string path);
	Fan(FanDriver *driver);
	virtual ~Fan() = default;
};


class TpFan : public Fan {
public:
	TpFan(std::string path);
};


class PwmFan : public Fan {
public:
	PwmFan(string path);
};


}

#endif /* THINKFAN_CONFIG_H_ */
