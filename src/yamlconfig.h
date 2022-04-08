#ifndef THINKFAN_YAMLCONFIG_H_
#define THINKFAN_YAMLCONFIG_H_

#include "thinkfan.h"
#include "wtf_ptr.h"

#include <vector>
#include <yaml-cpp/yaml.h>


namespace YAML {

using namespace thinkfan;


const string kw_sensors("sensors");
const string kw_fans("fans");
const string kw_levels("levels");
const string kw_tpacpi("tpacpi");
const string kw_hwmon("hwmon");
#ifdef USE_NVML
const string kw_nvidia("nvml");
#endif
#ifdef USE_ATASMART
const string kw_atasmart("atasmart");
#endif
#ifdef USE_LM_SENSORS
const string kw_chip("chip");
const string kw_ids("ids");
#endif
const string kw_speed("speed");
const string kw_upper("upper_limit");
const string kw_lower("lower_limit");
const string kw_name("name");
const string kw_indices("indices");
const string kw_correction("correction");
const string kw_optional("optional");
const string kw_max_errors("max_errors");


template<>
struct convert<wtf_ptr<thinkfan::Config>> {
	static bool decode(const Node &node, wtf_ptr<thinkfan::Config> &config);
};


struct LevelEntry {
	vector<pair<string, int>> fan_levels;
	vector<int> lower_limit;
	vector<int> upper_limit;
};


}


#endif // THINKFAN_YAMLCONFIG_H_
