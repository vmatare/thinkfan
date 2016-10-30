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


static vector<int> filter_indices;


static int filter_dirents(const struct dirent *entry)
{
	if (entry->d_type == DT_REG) {
		int temp_idx;
		if (sscanf(entry->d_name, "temp%u_input", &temp_idx) == 1
				// Apparently sscanf ignores everything after %u, so check again:
				// TODO: rewrite this as C++ style stream input
				&& (string("temp") + to_string(temp_idx) + "_input" == entry->d_name)
				&& std::find(filter_indices.begin(),
						filter_indices.end(),
						temp_idx) != filter_indices.end())
				return 1;
	}
	if ((entry->d_type & (DT_DIR | DT_LNK)) && !strncmp("hwmon", entry->d_name, 5))
		return 1;

	return 0;
}


static vector<wtf_ptr<HwmonSensorDriver>> find_hwmons(string path, vector<int> &&indices)
{
	vector<wtf_ptr<HwmonSensorDriver>> rv;

	unsigned char depth = 0;
	const unsigned char max_depth = 3;

	filter_indices = indices;

	while (filter_indices.size() > 0 && depth <= max_depth) {
		struct dirent **entries;
		int nentries = scandir(path.c_str(), &entries, filter_dirents, alphasort);
		if (nentries < 0)
			throw IOerror("Error scanning " + path + ": ", errno);
		if (nentries == 0)
			throw ConfigError("Could not find an `hwmon*' directory or `temp*_input' file in " + path + ".");

		for (int i = 0; i < nentries; i++) {
			int temp_idx;
			if (sscanf(entries[i]->d_name, "temp%u_input", &temp_idx) == 1) {
				// found temperature input files...
				auto it = std::find(
						filter_indices.begin(), filter_indices.end(), temp_idx);

				// *it Should have been put there by filter_dirents()
				assert(it != filter_indices.end());
				rv.push_back(make_wtf<HwmonSensorDriver>(path + "/" + entries[i]->d_name));
				filter_indices.erase(it);
				// stop crawling at this level
				depth = std::numeric_limits<unsigned char>::max();
			}
		}

		if (depth <= max_depth && !strncmp("hwmon", entries[0]->d_name, 5)) {
			(path += "/") += entries[0]->d_name;
			depth++;
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
		if (!node["hwmon"])
			return false;

		auto initial_size = sensors.size();
		string path = node["hwmon"].as<string>();
		vector<int> correction;
		if (node["correction"])
			correction = node["correction"].as<vector<int>>();

		if (node["temps"]) {
			vector<wtf_ptr<HwmonSensorDriver>> hwmons = find_hwmons(path, node["temps"].as<vector<int>>());
			if (!correction.empty()) {
				if (correction.size() != hwmons.size())
					throw YamlError(node["temps"].Mark(), MSG_CONF_CORRECTION_LEN(path, correction.size(), hwmons.size()));
				auto it = correction.begin();
				std::for_each(hwmons.begin(), hwmons.end(), [&] (wtf_ptr<HwmonSensorDriver> &sensor) {
					sensor->set_correction(vector<int>(1, *it++));
				});
			}
			for (wtf_ptr<HwmonSensorDriver> &h : hwmons)
				sensors.push_back(std::move(h));
		}
		else
			sensors.push_back(make_wtf<HwmonSensorDriver>(path));

		return sensors.size() > initial_size;
	}
};


template<>
struct convert<wtf_ptr<TpSensorDriver>> {
	static bool decode(const Node &node, wtf_ptr<TpSensorDriver> &sensor)
	{
		if (!node["tp_thermal"])
			return false;

		if (node["temps"]) {
			sensor = make_wtf<TpSensorDriver>(
						node["tp_thermal"].as<string>(),
					node["temps"].as<vector<int>>());
		}
		else
			sensor = make_wtf<TpSensorDriver>(node["tp_thermal"].as<string>());

		return true;
	}
};


template<>
struct convert<vector<wtf_ptr<const SensorDriver>>> {
	static bool decode(const Node &node, vector<wtf_ptr<const SensorDriver>> &sensors)
	{
		auto initial_size = sensors.size();
		if (!node.IsSequence())
			throw YamlError(node.Mark(), "Sensor entries must be a sequence. Forgot the dashes?");
		for (Node::const_iterator it = node.begin(); it != node.end(); ++it) {
			try {
				for (wtf_ptr<HwmonSensorDriver> &h : it->as<vector<wtf_ptr<HwmonSensorDriver>>>())
					sensors.push_back(std::move(h));
			} catch (YAML::BadConversion &) {
				sensors.push_back(it->as<wtf_ptr<TpSensorDriver>>());
			}
		}

		return sensors.size() > initial_size;
	}
};


template<>
struct convert<wtf_ptr<TpFanDriver>> {
	static bool decode(const Node &node, wtf_ptr<TpFanDriver> &fan)
	{
		if (!node["tp_fan"])
			return false;

		fan = make_wtf<TpFanDriver>(node["tp_fan"].as<string>());
		return true;
	}
};


template<>
struct convert<wtf_ptr<HwmonFanDriver>> {
	static bool decode(const Node &node, wtf_ptr<HwmonFanDriver> &fan)
	{
		if (!node["pwm_fan"])
			return false;

		fan = make_wtf<HwmonFanDriver>(node["pwm_fan"].as<string>());
		return true;
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
				fans.push_back(it->as<wtf_ptr<HwmonFanDriver>>());
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
		return make_wtf<LevelT>(node["speed"].as<int>(), lower, upper);
	} catch (BadConversion &) {
		return make_wtf<LevelT>(node["speed"].as<string>(), lower, upper);
	}
}


template<>
struct convert<wtf_ptr<ComplexLevel>> {
	static bool decode(const Node &node, wtf_ptr<ComplexLevel> &level)
	{
		const Node &n_lower = node["lower_limit"], &n_upper = node["upper_limit"];
		if (!(node["speed"] && (n_lower || n_upper)))
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
		const Node &n_lower = node["lower_limit"], &n_upper = node["upper_limit"];
		if (!(node["speed"] && (n_lower || n_upper)))
			return false;

		int lower, upper;
		if (n_lower) {
			lower = n_lower.as<int>();
			if (lower == numeric_limits<int>::min())
				throw BadConversion(n_lower.Mark());
		}
		if (n_upper) {
			upper = n_upper.as<int>();
			if (upper == numeric_limits<int>::max())
				throw BadConversion(n_upper.Mark());
		}

		if (!n_lower)
			lower = numeric_limits<int>::min();
		else if (!n_upper)
			upper = numeric_limits<int>::max();

		level = make_level<SimpleLevel>(node, lower, upper);
		return true;
	}
};


template<>
struct convert<vector<wtf_ptr<const Level>>> {
	static bool decode(const Node &node, vector<wtf_ptr<const Level>> &levels)
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
	for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
		config = make_wtf<Config>();
		const Node &entry = it->second;
		const string key = it->first.as<string>();
		if (key == "sensors") {
			auto sensors = entry.as<vector<wtf_ptr<const SensorDriver>>>();
			for (wtf_ptr<const SensorDriver> &s : sensors) {
				config->add_sensor(unique_ptr<const SensorDriver>(s.release()));
			}
		} else if (key == "fans") {
				auto fans = entry.as<vector<wtf_ptr<FanDriver>>>();
				for (wtf_ptr<FanDriver> &f : fans) {
					config->add_fan(unique_ptr<FanDriver>(f.release()));
				}
		} else if (key == "levels") {
			auto levels = entry.as<vector<wtf_ptr<const Level>>>();
			std::sort(levels.begin(), levels.end(), [] (wtf_ptr<const Level> &lhs, wtf_ptr<const Level> &rhs) {
				return lhs->num() <= rhs->num();
			});
			for (vector<wtf_ptr<const Level>>::iterator lit = levels.begin(); lit != levels.end(); ++lit) {
				if (lit != levels.begin() && (*lit)->lower_limit().front() == std::numeric_limits<int>::min())
					error<YamlError>(it->first.Mark(), MSG_CONF_MISSING_LOWER_LIMIT);
				if (lit != --levels.end() && (*lit)->upper_limit().front() == std::numeric_limits<int>::max())
					error<YamlError>(it->first.Mark(), MSG_CONF_MISSING_UPPER_LIMIT);
				config->add_level(unique_ptr<const Level>(lit->release()));
			}
		} else {
			throw YamlError(it->first.Mark(), "Unknown keyword");
		}
	}
	return true;
}


}



