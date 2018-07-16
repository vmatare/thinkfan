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
static const string kw_name("name");
static const string kw_indices("indices");
static const string kw_correction("correction");
static const string kw_optional("optional");

#ifdef HAVE_OLD_YAMLCPP
static inline Mark get_mark_compat(const Node &)
{ return Mark::null_mark(); }

static inline BadConversion get_bad_conversion_compat(const Node &)
{ return BadConversion(); }
#else
static inline Mark get_mark_compat(const Node &node)
{ return node.Mark(); }

static inline BadConversion get_bad_conversion_compat(const Node &node)
{ return BadConversion(node.Mark()); }
#endif


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
	if ((entry->d_type == DT_DIR || entry->d_type == DT_LNK)	 && (
	            !strncmp("hwmon", entry->d_name, 5) || !strcmp("device", entry->d_name)))
		return 1;
	return 0;
}

static int filter_subdirs(const struct dirent *entry)
{
	return (entry->d_type & DT_DIR || entry->d_type == DT_LNK)
		&& string(entry->d_name) != "." && string(entry->d_name) != ".."
		&& string(entry->d_name) != "subsystem";
}


template<class T>
static int scandir(const string &path, struct dirent ***entries);

template<>
int scandir< HwmonSensorDriver > (const string &path, struct dirent ***entries)
{ return ::scandir(path.c_str(), entries, filter_temp_inputs, alphasort); }

template<>
int scandir< HwmonFanDriver > (const string &path, struct dirent ***entries)
{ return ::scandir(path.c_str(), entries, filter_pwms, alphasort); }


static vector<string> find_hwmons_by_name(string path, string name, unsigned char depth = 1) {
	const unsigned char max_depth = 5;
	vector<string> result;

	ifstream f(path + "/name");
	if (f.is_open() && f.good()) {
		string tmp;
		if ((f >> tmp) && tmp == name) {
			result.push_back(path);
			return result;
		}
	}
	if (depth >= max_depth) {
		return result;  // don't recurse to subdirs
	}

	struct dirent **entries;
	int nentries = ::scandir(path.c_str(), &entries, filter_subdirs, nullptr);
	if (nentries == -1) {
		return result;
	}
	for (int i = 0; i < nentries; i++) {
		auto subdir = path + "/" + entries[i]->d_name;
		free(entries[i]);

		struct stat statbuf;
		int err = stat(path.c_str(), &statbuf);
		if (err || (statbuf.st_mode & S_IFMT) != S_IFDIR)
			continue;

		auto found = find_hwmons_by_name(subdir, name, depth + 1);
		result.insert(result.end(), found.begin(), found.end());
	}
	free(entries);

	return result;
}


template<class T, class...ExtraArgTs>
static vector<wtf_ptr<T>> find_hwmons_by_indices(string path, const vector<int> &indices, ExtraArgTs... extra_args, unsigned char depth = 0)
{
	vector<wtf_ptr<T>> rv;

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

				rv.push_back(make_wtf<T>(path + "/" + entries[i]->d_name, extra_args...));
				filter_indices.erase(it);
				// stop crawling at this level
				depth = std::numeric_limits<unsigned char>::max();
			}
		}

		if (nentries == 0 || temp_idx < 0) {
			nentries = ::scandir(path.c_str(), &entries, filter_hwmon_dirs, alphasort);
			if (nentries < 0)
				throw IOerror("Error scanning " + path + ": ", errno);

			if (nentries > 0 && depth <= max_depth) {
				for (int i = 0; i < nentries && rv.empty(); i++)
					rv = find_hwmons_by_indices<T, ExtraArgTs...>(path + "/" + entries[i]->d_name, indices, extra_args..., depth + 1);
			}
			else
				throw ConfigError("Could not find an `hwmon*' directory or `temp*_input' file in " + path + ".");
		}

		for (int i = 0; i < nentries; i++)
			free(entries[i]);
		free(entries);
	}

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

		if (node[kw_name]) {
			auto paths = find_hwmons_by_name(path, node[kw_name].as<string>());
			if (paths.size() != 1) {
				string msg;
				if (paths.size() == 0) {
					msg = MSG_HWMON_NOT_FOUND;
				} else {
					msg = MSG_MULTIPLE_HWMONS_FOUND;
					for (string hwmon_path : paths) {
						msg += " " + hwmon_path;
					}
				}
				throw YamlError(get_mark_compat(node[kw_name]), msg);
			}
			path = paths[0];
		}

		bool optional = node[kw_optional] ? node[kw_optional].as<bool>() : false;

		if (node[kw_indices]) {
			vector<int> indices = node[kw_indices].as<vector<int>>();
			vector<wtf_ptr<HwmonSensorDriver>> hwmons = find_hwmons_by_indices<HwmonSensorDriver, bool>(path, indices, optional);
			if (indices.size() != hwmons.size())
				throw YamlError(get_mark_compat(node[kw_indices]), "Unable to find requested temperature inputs in " + path + ".");

			if (!correction.empty()) {
				if (correction.size() != hwmons.size())
					throw YamlError(
				            get_mark_compat(node[kw_indices]),
				            MSG_CONF_CORRECTION_LEN(path, correction.size(), hwmons.size()));
				auto it = correction.begin();
				std::for_each(hwmons.begin(), hwmons.end(), [&] (wtf_ptr<HwmonSensorDriver> &sensor) {
					sensor->set_correction(vector<int>(1, *it++));
				});
			}
			for (wtf_ptr<HwmonSensorDriver> &h : hwmons) {
				sensors.push_back(std::move(h));
			}
		}
		else {
			wtf_ptr<HwmonSensorDriver> h = make_wtf<HwmonSensorDriver>(path, optional, correction);
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

		bool optional = node[kw_optional] ? node[kw_optional].as<bool>() : false;

		if (node[kw_indices]) {
			sensor = make_wtf<TpSensorDriver>(
			            node[kw_tpacpi].as<string>(),
			            optional,
			            node[kw_indices].as<vector<unsigned int>>(),
			            correction);
		}
		else
			sensor = make_wtf<TpSensorDriver>(node[kw_tpacpi].as<string>(), optional, correction);

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

		bool optional = node[kw_optional] ? node[kw_optional].as<bool>() : false;

		sensor = make_wtf<NvmlSensorDriver>(node[kw_nvidia].as<string>(), optional, correction);

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

		bool optional = node[kw_optional] ? node[kw_optional].as<bool>() : false;

		sensor = make_wtf<AtasmartSensorDriver>(node["atasmart"].as<string>(), optional, correction);

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
			throw YamlError(get_mark_compat(node), "Sensor entries must be a sequence. Forgot the dashes?");
		for (Node::const_iterator it = node.begin(); it != node.end(); ++it) {
			if ((*it)[kw_hwmon])
				for (wtf_ptr<HwmonSensorDriver> h : it->as<vector<wtf_ptr<HwmonSensorDriver>>>())
					sensors.push_back(std::move(h));
			else if ((*it)[kw_tpacpi]) {
				wtf_ptr<TpSensorDriver> tmp = it->as<wtf_ptr<TpSensorDriver>>();
				sensors.push_back(std::move(tmp));
			}
#ifdef USE_NVML
			else if ((*it)[kw_nvidia]) {
				wtf_ptr<NvmlSensorDriver> tmp = it->as<wtf_ptr<NvmlSensorDriver>>();
				sensors.push_back(std::move(tmp));
			}
#endif //USE_NVML
#ifdef USE_ATASMART
			else if ((*it)[kw_atasmart]) {
				wtf_ptr<AtasmartSensorDriver> tmp = it->as<wtf_ptr<AtasmartSensorDriver>>();
				sensors.push_back(std::move(tmp));
			}
#endif //USE_ATASMART
			else
				throw YamlError(get_mark_compat(*it), "Invalid sensor entry");
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

		if (node[kw_name]) {
			auto paths = find_hwmons_by_name(path, node[kw_name].as<string>());
			if (paths.size() != 1) {
				string msg;
				if (paths.size() == 0) {
					msg = MSG_HWMON_NOT_FOUND;
				} else {
					msg = MSG_MULTIPLE_HWMONS_FOUND;
					for (string hwmon_path : paths) {
						msg += " " + hwmon_path;
					}
				}
				throw YamlError(get_mark_compat(node[kw_name]), msg);
			}
			path = paths[0];
		}

		if (node[kw_indices]) {
			vector<int> indices = node[kw_indices].as<vector<int>>();
			fans = find_hwmons_by_indices<HwmonFanDriver>(path, indices);
			if (indices.size() != fans.size())
				throw YamlError(get_mark_compat(node[kw_indices]), "Unable to find requested PWM controls in " + path + ".");
		}
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
			throw YamlError(get_mark_compat(node), "Fan entries must be a sequence. Forgot the dashes?");
		for (Node::const_iterator it = node.begin(); it != node.end(); ++it) {
			try {
				for (auto f : it->as<vector<wtf_ptr<HwmonFanDriver>>>())
					fans.push_back(std::move(f));
			} catch (YAML::BadConversion &) {
				wtf_ptr<TpFanDriver> tmp = it->as<wtf_ptr<TpFanDriver>>();
				fans.push_back(std::move(tmp));
			}
		}
		return fans.size() > initial_size;
	}
};


template<>
struct convert<LevelEntry> {
	static bool decode(const Node &node, LevelEntry &rv)
	{
		const Node &n_lower = node[kw_lower], &n_upper = node[kw_upper];
		if (!(node[kw_speed] && (n_lower || n_upper)))
			return false;

		if (node[kw_speed].IsSequence()) {
			for (YAML::const_iterator it = node[kw_speed].begin(); it != node[kw_speed].end(); ++it) {
				int level_n;
				string level_s;
				try {
					level_n = it->second.as<int>();
					if (level_n == 127)
						level_s = "level disengaged";
					else
						level_s = "level " + std::to_string(level_n);
				} catch (std::bad_cast &) {
					level_s = it->second.as<string>();
					level_n = Level::string_to_int(level_s);
				}
				rv.fan_levels.push_back({level_s, level_n});
			}
		}

		if (n_lower) {
			vector<int> lower = n_lower.as<vector<int>>();
			if (std::find(lower.begin(), lower.end(), numeric_limits<int>::min()) != lower.end())
				throw get_bad_conversion_compat(n_lower);
			rv.lower_limit = std::move(lower);
		}
		if (n_upper) {
			vector<int> upper = n_upper.as<vector<int>>();
			if (std::find(upper.begin(), upper.end(), numeric_limits<int>::max()) != upper.end())
				throw get_bad_conversion_compat(n_upper);
			rv.upper_limit = std::move(upper);
		}

		if (rv.lower_limit.empty())
			rv.lower_limit = vector<int>(rv.upper_limit.size(), numeric_limits<int>::min());
		else if (rv.upper_limit.empty())
			rv.upper_limit = vector<int>(rv.lower_limit.size(), numeric_limits<int>::max());

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
		}
		else {
			int lower, upper;

			if (n_lower) {
				lower = n_lower.as<int>();
				if (lower == numeric_limits<int>::min())
					throw get_bad_conversion_compat(n_lower);
			}
			else
				lower = numeric_limits<int>::min();

			if (n_upper) {
				upper = n_upper.as<int>();
				if (upper == numeric_limits<int>::max())
					throw get_bad_conversion_compat(n_upper);
			}
			else
				upper = numeric_limits<int>::max();

			try {
				level = make_wtf<SimpleLevel>(node[kw_speed].as<int>(), lower, upper);
			} catch (BadConversion &) {
				level = make_wtf<SimpleLevel>(node[kw_speed].as<string>(), lower, upper);
			}
		}
		return true;
	}
};



bool convert<wtf_ptr<Config>>::decode(const Node &node, wtf_ptr<Config> &config)
{
	if (!node.size())
		throw ParserException(get_mark_compat(node), "Invalid YAML syntax");

	config = make_wtf<Config>();
	vector<wtf_ptr<FanDriver>> fans;

	for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
		const Node entry = it->second;
		const string key = it->first.as<string>();
		if (key == kw_sensors) {
			auto sensors = entry.as<vector<wtf_ptr<SensorDriver>>>();
			for (wtf_ptr<SensorDriver> &s : sensors) {
				config->add_sensor(unique_ptr<SensorDriver>(s.release()));
			}
		} else if (key == kw_fans) {
			fans = entry.as<vector<wtf_ptr<FanDriver>>>();
		} else if (key == kw_levels) {
			if (!entry.IsSequence())
				throw YamlError(get_mark_compat(node), "Level entries must be a sequence. Forgot the dashes?");

			for (YAML::const_iterator it_lvl = entry.begin(); it_lvl != entry.end(); ++it_lvl) {
				try {
					LevelEntry lvl_entry = it_lvl->as<LevelEntry>();

					if (lvl_entry.fan_levels.size() != fans.size())
						throw YamlError(get_mark_compat(it_lvl->second),
							"Number of " + kw_speed + " entries doesn't match number of " + kw_fans
						);

					if (it_lvl != entry.begin() && lvl_entry.lower_limit.empty())
						error<YamlError>(get_mark_compat(it->first), MSG_CONF_MISSING_LOWER_LIMIT);
					YAML::const_iterator it_tmp = it_lvl;
					if (++it_tmp != entry.end() && lvl_entry.upper_limit.empty())
						error<YamlError>(get_mark_compat(it->first), MSG_CONF_MISSING_UPPER_LIMIT);

					size_t fan_idx = 0;
					for (wtf_ptr<FanDriver> &fan : fans) {
						unique_ptr<StepwiseMapping> fan_cfg = std::make_unique<StepwiseMapping>();
						fan_cfg->set_fan(unique_ptr<FanDriver>(fan.release()));
						fan_cfg->add_level(
							std::make_unique<ComplexLevel>(
								lvl_entry.fan_levels[fan_idx].first,
								lvl_entry.lower_limit,
								lvl_entry.upper_limit
							)
						);
						++fan_idx;
					}
				} catch (YAML::BadConversion &) {
					unique_ptr<SimpleLevel> simple_lvl(it_lvl->as<wtf_ptr<SimpleLevel>>().release());
				}
			}
		} else {
			throw YamlError(get_mark_compat(it->first), "Unknown keyword");
		}
	}

	return true;
}


}



