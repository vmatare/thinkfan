#include "yamlconfig.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fnmatch.h>
#include <cstdio>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <tuple>
#include <memory>

#include "message.h"


namespace YAML {

using namespace thinkfan;
using namespace std;

static const string kw_sensors("sensors");
static const string kw_fans("fans");
static const string kw_levels("levels");
static const string kw_tpacpi("tpacpi");
static const string kw_hwmon("hwmon");
#ifdef USE_NVML
static const string kw_nvidia("nvidia");
#endif
#ifdef USE_ATASMART
static const string kw_atasmart("atasmart");
#endif
static const string kw_speed("speed");
static const string kw_upper("upper_limit");
static const string kw_lower("lower_limit");
static const string kw_indices("indices");
static const string kw_correction("correction");


static int get_index(const string &fname, const string &pfx, const string &sfx)
{
	int rv = -1;
	string::size_type i = fname.rfind(sfx);
	if (fname.substr(0, pfx.length()) == pfx && i != string::npos) {
		try {
			size_t idx_len;
			rv = stoi(fname.substr(pfx.length()), &idx_len, 10);
			if (fname.substr(pfx.length() + idx_len) != sfx)
				return -1;
		} catch (...)
		{}
	}

	return rv;
}

template<class T> static int get_index(const string &fname);

template<> int get_index<HwmonSensorDriver>(const string &fname)
{ return get_index(fname, "temp", "_input"); }

template<> int get_index<HwmonFanDriver>(const string &fname)
{ return get_index(fname, "pwm", ""); }


static vector<int> filter_indices;

static int file_filter(const struct dirent *entry, const string &pfx, const string &sfx)
{
	if (entry->d_type == DT_REG) {
		int idx = get_index(entry->d_name, pfx, sfx);
		if (idx >= 0 && std::find(filter_indices.begin(),
						filter_indices.end(),
						idx) != filter_indices.end())
				return 1;
	}
	return 0;
}

static int filter_temp_inputs(const struct dirent *entry)
{ return file_filter(entry, "temp", "_input"); }

static int filter_pwms(const struct dirent *entry)
{ return file_filter(entry, "pwm", ""); }


static int filter_hwmon_dirs(const struct dirent *entry)
{
	if ((entry->d_type & (DT_DIR | DT_LNK)) && (
				!strncmp("hwmon", entry->d_name, 5) || !strcmp("device", entry->d_name)))
		return 1;
	return 0;
}


template<class T>
static int scandir(const string &path, struct dirent ***entries);

template<>
int scandir< HwmonSensorDriver > (const string &path, struct dirent ***entries)
{ return ::scandir(path.c_str(), entries, filter_temp_inputs, alphasort); }

template<>
int scandir< HwmonFanDriver > (const string &path, struct dirent ***entries)
{ return ::scandir(path.c_str(), entries, filter_pwms, alphasort); }



template<class T>
static vector<wtf_ptr<T>> find_hwmons(string path, vector<int> &&indices)
{
	vector<wtf_ptr<T>> rv;

	unsigned char depth = 0;
	const unsigned char max_depth = 3;

	filter_indices = indices;

	while (filter_indices.size() > 0 && depth <= max_depth) {
		struct dirent **entries;
		int nentries = scandir<T>(path, &entries);

		if (nentries < 0)
			throw IOerror("Error scanning " + path + ": ", errno);

		int temp_idx = -1;

		if (nentries > 0) {
			for (int i = 0; i < nentries; i++) {
				temp_idx = get_index<T>(entries[i]->d_name);
				if (temp_idx < 0)
					break; // no index found in file name

				auto it = std::find(filter_indices.begin(), filter_indices.end(), temp_idx);

				rv.push_back(make_wtf<T>(path + "/" + entries[i]->d_name));
				filter_indices.erase(it);
				// stop crawling at this level
				depth = std::numeric_limits<unsigned char>::max();
			}
		}

		if (nentries == 0 || temp_idx < 0) {
			nentries = ::scandir(path.c_str(), &entries, filter_hwmon_dirs, alphasort);
			if (nentries < 0)
				throw IOerror("Error scanning " + path + ": ", errno);
			if (nentries == 0)
				throw ConfigError("Could not find an `hwmon*' directory or `temp*_input' file in " + path + ".");

			if (depth <= max_depth && !strncmp(kw_hwmon.c_str(), entries[0]->d_name, 5)) {
				(path += "/") += entries[0]->d_name;
				depth++;
			}
		}

		for (int i = 0; i < nentries; i++)
			free(entries[i]);
		free(entries);
	}

	if (!filter_indices.empty())
		throw ConfigError("Unable to find all requested temperature inputs in " + path + ".");

	return rv;
}


template<>
struct convert<vector<wtf_ptr<HwmonSensorDriver>>> {
	static bool decode(const Node &node, vector<wtf_ptr<HwmonSensorDriver>> &sensors)
	{
		if (!node[kw_hwmon])
			return false;

		auto initial_size = sensors.size();
		string path = node[kw_hwmon].as<string>();

		vector<int> correction;
		if (node[kw_correction])
			correction = node[kw_correction].as<vector<int>>();

		if (node[kw_indices]) {
			vector<wtf_ptr<HwmonSensorDriver>> hwmons = find_hwmons<HwmonSensorDriver>(
						path, node[kw_indices].as<vector<int>>());
			if (!correction.empty()) {
				if (correction.size() != hwmons.size())
					throw YamlError(node[kw_indices].Mark(), MSG_CONF_CORRECTION_LEN(
										path, correction.size(), hwmons.size()));
				auto it = correction.begin();
				std::for_each(hwmons.begin(), hwmons.end(), [&] (wtf_ptr<HwmonSensorDriver> &sensor) {
					sensor->set_correction(vector<int>(1, *it++));
				});
			}
			for (wtf_ptr<HwmonSensorDriver> &h : hwmons)
				sensors.push_back(std::move(h));
		}
		else {
			wtf_ptr<HwmonSensorDriver> h = make_wtf<HwmonSensorDriver>(path, correction);
			sensors.push_back(h);
		}

		return sensors.size() > initial_size;
	}
};


template<>
struct convert<wtf_ptr<TpSensorDriver>> {
	static bool decode(const Node &node, wtf_ptr<TpSensorDriver> &sensor)
	{
		if (!node[kw_tpacpi])
			return false;

		vector<int> correction;
		if (node[kw_correction])
			correction = node[kw_correction].as<vector<int>>();

		if (node[kw_indices]) {
			sensor = make_wtf<TpSensorDriver>(
						node[kw_tpacpi].as<string>(),
						node[kw_indices].as<vector<unsigned int>>(),
						correction);
		}
		else
			sensor = make_wtf<TpSensorDriver>(node[kw_tpacpi].as<string>(), correction);

		return true;
	}
};


#ifdef USE_NVML
template<>
struct convert<wtf_ptr<NvmlSensorDriver>> {
	static bool decode(const Node &node, wtf_ptr<NvmlSensorDriver> &sensor)
	{
		if (!node[kw_nvidia])
			return false;

		vector<int> correction;
		if (node[kw_correction])
			correction = node[kw_correction].as<vector<int>>();

		sensor = make_wtf<NvmlSensorDriver>(node[kw_nvidia].as<string>(), correction);
		return true;
	}
};
#endif //USE_NVML


#ifdef USE_ATASMART
template<>
struct convert<wtf_ptr<AtasmartSensorDriver>> {
	static bool decode(const Node &node, wtf_ptr<AtasmartSensorDriver> &sensor)
	{
		if (!node["atasmart"])
			return false;

		vector<int> correction;
		if (node[kw_correction])
			correction = node[kw_correction].as<vector<int>>();

		sensor = make_wtf<AtasmartSensorDriver>(node["atasmart"].as<string>(), correction);
		return true;
	}
};
#endif //USE_ATASMART


template<>
struct convert<vector<wtf_ptr<SensorDriver>>> {

	static bool decode(const Node &node, vector<wtf_ptr<SensorDriver>> &sensors)
	{
		auto initial_size = sensors.size();
		if (!node.IsSequence())
			throw YamlError(node.Mark(), "Sensor entries must be a sequence. Forgot the dashes?");
		for (Node::const_iterator it = node.begin(); it != node.end(); ++it) {
			if ((*it)[kw_hwmon])
				for (wtf_ptr<HwmonSensorDriver> &h : it->as<vector<wtf_ptr<HwmonSensorDriver>>>())
					sensors.push_back(std::move(h));
			else if ((*it)[kw_tpacpi])
				sensors.push_back(it->as<wtf_ptr<TpSensorDriver>>());
#ifdef USE_NVML
			else if ((*it)[kw_nvidia])
				sensors.push_back(it->as<wtf_ptr<NvmlSensorDriver>>());
#endif //USE_NVML
#ifdef USE_ATASMART
			else if ((*it)[kw_atasmart])
				sensors.push_back(it->as<wtf_ptr<AtasmartSensorDriver>>());
#endif //USE_ATASMART
			else
				throw YamlError(it->Mark(), "Invalid sensor entry");
		}

		return sensors.size() > initial_size;
	}
};


template<>
struct convert<wtf_ptr<TpFanDriver>> {
	static bool decode(const Node &node, wtf_ptr<TpFanDriver> &fan)
	{
		if (!node[kw_tpacpi])
			return false;

		fan = make_wtf<TpFanDriver>(node[kw_tpacpi].as<string>());
		return true;
	}
};


template<>
struct convert<vector<wtf_ptr<HwmonFanDriver>>> {
	static bool decode(const Node &node, vector<wtf_ptr<HwmonFanDriver>> &fans)
	{
		if (!node[kw_hwmon])
			return false;

		auto initial_size = fans.size();
		string path = node[kw_hwmon].as<string>();

		if (node[kw_indices])
			fans = find_hwmons<HwmonFanDriver>(path, node[kw_indices].as<vector<int>>());
		else
			fans.push_back(make_wtf<HwmonFanDriver>(node[kw_hwmon].as<string>()));

		return fans.size() > initial_size;
	}
};


template<>
struct convert<vector<wtf_ptr<FanDriver>>> {
	static bool decode(const Node &node, vector<wtf_ptr<FanDriver>> &fans)
	{
		auto initial_size = fans.size();
		if (!node.IsSequence())
			throw YamlError(node.Mark(), "Fan entries must be a sequence. Forgot the dashes?");
		for (Node::const_iterator it = node.begin(); it != node.end(); ++it) {
			try {
				for (auto &f : it->as<vector<wtf_ptr<HwmonFanDriver>>>())
					fans.push_back(std::move(f));
			} catch (YAML::BadConversion &) {
				fans.push_back(it->as<wtf_ptr<TpFanDriver>>());
			}
		}
		return fans.size() > initial_size;
	}
};


template<class LevelT, class LimitT>
static wtf_ptr<LevelT> make_level(const Node &node, LimitT lower, LimitT upper)
{
	try {
		return make_wtf<LevelT>(node[kw_speed].as<int>(), lower, upper);
	} catch (BadConversion &) {
		return make_wtf<LevelT>(node[kw_speed].as<string>(), lower, upper);
	}
}


template<>
struct convert<wtf_ptr<ComplexLevel>> {
	static bool decode(const Node &node, wtf_ptr<ComplexLevel> &level)
	{
		const Node &n_lower = node[kw_lower], &n_upper = node[kw_upper];
		if (!(node[kw_speed] && (n_lower || n_upper)))
			return false;

		vector<int> lower, upper;
		if (n_lower) {
			lower = n_lower.as<vector<int>>();
			if (std::find(lower.begin(), lower.end(), numeric_limits<int>::min()) != lower.end())
				throw BadConversion(n_lower.Mark());
		}
		if (n_upper) {
			upper = n_upper.as<vector<int>>();
			if (std::find(upper.begin(), upper.end(), numeric_limits<int>::max()) != upper.end())
				throw BadConversion(n_upper.Mark());
		}

		if (lower.empty())
			lower = vector<int>(upper.size(), numeric_limits<int>::min());
		else if (upper.empty())
			upper = vector<int>(lower.size(), numeric_limits<int>::max());

		level = make_level<ComplexLevel>(node, lower, upper);

		return true;
	}
};


template<>
struct convert<wtf_ptr<SimpleLevel>> {
	static bool decode(const Node &node, wtf_ptr<SimpleLevel> &level)
	{
		const Node &n_lower = node[kw_lower], &n_upper = node[kw_upper];

		if (!(node[kw_speed] && (n_lower || n_upper))) {
			if (!node.IsSequence())
				return false;

			try {
				level = make_wtf<SimpleLevel>(node[0].as<int>(), node[1].as<int>(), node[2].as<int>());
			} catch (BadConversion &) {
				level = make_wtf<SimpleLevel>(node[0].as<string>(), node[1].as<int>(), node[2].as<int>());
			}

			return true;
		}
		else {
			int lower, upper;

			if (n_lower) {
				lower = n_lower.as<int>();
				if (lower == numeric_limits<int>::min())
					throw BadConversion(n_lower.Mark());
			}
			else
				lower = numeric_limits<int>::min();

			if (n_upper) {
				upper = n_upper.as<int>();
				if (upper == numeric_limits<int>::max())
					throw BadConversion(n_upper.Mark());
			}
			else
				upper = numeric_limits<int>::max();

			level = make_level<SimpleLevel>(node, lower, upper);
			return true;
		}
	}
};


template<>
struct convert<vector<wtf_ptr<Level>>> {
	static bool decode(const Node &node, vector<wtf_ptr<Level>> &levels)
	{
		auto initial_size = levels.size();
		if (!node.IsSequence())
			throw YamlError(node.Mark(), "Level entries must be a sequence. Forgot the dashes?");
		for (const Node &n_level : node) {
			try {
				levels.push_back(n_level.as<wtf_ptr<ComplexLevel>>());
			} catch (YAML::BadConversion &) {
				levels.push_back(n_level.as<wtf_ptr<SimpleLevel>>());
			}
		}
		return levels.size() > initial_size;
	}
};



bool convert<wtf_ptr<Config>>::decode(const Node &node, wtf_ptr<Config> &config)
{
	if (!node.size())
		throw ParserException(node.Mark(), "Invalid YAML syntax");

	config = make_wtf<Config>();

	for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
		const Node &entry = it->second;
		const string key = it->first.as<string>();
		if (key == kw_sensors) {
			auto sensors = entry.as<vector<wtf_ptr<SensorDriver>>>();
			for (wtf_ptr<SensorDriver> &s : sensors) {
				config->add_sensor(unique_ptr<SensorDriver>(s.release()));
			}
		} else if (key == kw_fans) {
			auto fans = entry.as<vector<wtf_ptr<FanDriver>>>();
			for (wtf_ptr<FanDriver> &f : fans) {
				config->add_fan(unique_ptr<FanDriver>(f.release()));
			}
		} else if (key == kw_levels) {
			auto levels = entry.as<vector<wtf_ptr<Level>>>();
			std::sort(levels.begin(), levels.end(), [] (wtf_ptr<Level> &lhs, wtf_ptr<Level> &rhs) {
				return lhs->num() <= rhs->num();
			});
			for (vector<wtf_ptr<Level>>::iterator lit = levels.begin(); lit != levels.end(); ++lit) {
				if (lit != levels.begin() && (*lit)->lower_limit().front() == std::numeric_limits<int>::min())
					error<YamlError>(it->first.Mark(), MSG_CONF_MISSING_LOWER_LIMIT);
				if (lit != --levels.end() && (*lit)->upper_limit().front() == std::numeric_limits<int>::max())
					error<YamlError>(it->first.Mark(), MSG_CONF_MISSING_UPPER_LIMIT);
				config->add_level(unique_ptr<Level>(lit->release()));
			}
		} else {
			throw YamlError(it->first.Mark(), "Unknown keyword");
		}
	}
	return true;
}


}



