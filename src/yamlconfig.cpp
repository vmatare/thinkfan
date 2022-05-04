#include "yamlconfig.h"
#include "error.h"
#include "driver.h"
#include "config.h"
#include "fans.h"
#include "sensors.h"

#include <tuple>
#include <memory>

#include "message.h"
#include "hwmon.h"


namespace YAML {

using namespace thinkfan;


/*
#ifdef HAVE_OLD_YAMLCPP
Mark get_mark_compat(const Node &);
BadConversion get_bad_conversion_compat(const Node &);
#else
Mark get_mark_compat(const Node &node);
BadConversion get_bad_conversion_compat(const Node &node);
#endif
*/



#ifdef HAVE_OLD_YAMLCPP
Mark get_mark_compat(const Node &)
{ return Mark::null_mark(); }

BadConversion get_bad_conversion_compat(const Node &)
{ return BadConversion(); }
#else
Mark get_mark_compat(const Node &node)
{ return node.Mark(); }

BadConversion get_bad_conversion_compat(const Node &node)
{ return BadConversion(node.Mark()); }
#endif


template<class DriverT>
bool convert_driver(const Node &node, DriverT &driver);


template<class DriverT>
struct convert_base {
	static bool decode(const Node &node, DriverT &driver)
	{
		try {
			return convert_driver<DriverT>(node, driver);
		} catch (ConfigError &e) {
			throw YamlError(get_mark_compat(node), e.reason());
		}
	}
};


// Have to explicitly specialize these because libyaml-cpp comes with a vector<T> specialization
// that would be used otherwise
template<>
struct convert<vector<wtf_ptr<HwmonSensorDriver>>>
: convert_base<vector<wtf_ptr<HwmonSensorDriver>>>
{};

template<>
struct convert<vector<wtf_ptr<HwmonFanDriver>>>
: convert_base<vector<wtf_ptr<HwmonFanDriver>>>
{};

// The rest we can just blindly pass through because there is no competing impl
template<class DriverT>
struct convert
: convert_base<DriverT>
{};


template<class T>
opt<T> decode_opt(const Node &node) {
	if (node)
		return node.as<T>();
	else
		return nullopt;
}


template<>
bool convert_driver<vector<wtf_ptr<HwmonSensorDriver>>>(
	const Node &node,
	vector<wtf_ptr<HwmonSensorDriver>> &sensors
) {
	if (!node[kw_hwmon])
		return false;

	string path = node[kw_hwmon].as<string>();

	opt<vector<int>> correction = decode_opt<vector<int>>(node[kw_correction]);
	opt<const string> name = decode_opt<string>(node[kw_name]);
	bool optional = node[kw_optional] ? node[kw_optional].as<bool>() : false;
	opt<unsigned int> max_errors = decode_opt<unsigned int>(node[kw_max_errors]);
	opt<vector<unsigned int>> indices = decode_opt<vector<unsigned int>>(node[kw_indices]);

	auto hwmon_iface = std::make_shared<HwmonInterface<SensorDriver>>(path, name, indices);

	if (indices) {
		if (correction && correction->size() != indices->size())
			throw YamlError(
				get_mark_compat(node[kw_indices]),
				MSG_CONF_CORRECTION_LEN(path, correction->size(), indices->size())
			);
	}
	else {
		if (optional)
			throw YamlError(
				get_mark_compat(node),
				"An optional hwmon sensor must have an '"+kw_indices+"' entry so thinkfan knows how many temperatures to expect."
			);
		if (correction && correction->size() != 1)
			throw YamlError(
				get_mark_compat(node[kw_correction]),
				"If no indices are specified, the '"+kw_hwmon+"' path must refer to a specific temp*_input file "
				"and therefore the length of '"+kw_correction+"' must be 1"
			);
	}

	for (unsigned int i = 0; i < (indices ? indices->size() : 1); ++i) {
		wtf_ptr<HwmonSensorDriver> drv(new HwmonSensorDriver(
			hwmon_iface,
			optional,
			correction ? opt<int>(correction.value()[i++]) : nullopt,
			max_errors
		));
		sensors.push_back(drv);
	}

	return true;
}


template<>
bool convert_driver<wtf_ptr<TpSensorDriver>>(const Node &node, wtf_ptr<TpSensorDriver> &sensor)
{
	if (!node[kw_tpacpi])
		return false;

	opt<vector<int>> correction = decode_opt<vector<int>>(node[kw_correction]);
	opt<vector<unsigned int>> indices = decode_opt<vector<unsigned int>>(node[kw_indices]);
	bool optional = node[kw_optional] ? node[kw_optional].as<bool>() : false;
	opt<unsigned int> max_errors = decode_opt<unsigned int>(node[kw_max_errors]);

	if (!indices && optional)
		throw YamlError(
			get_mark_compat(node),
			"An optional hwmon sensor must have an \"indices\" entry so thinkfan knows how many temperatures to expect."
		);

	sensor = wtf_ptr<TpSensorDriver>(new TpSensorDriver(
		node[kw_tpacpi].as<string>(),
		optional,
		indices,
		correction,
		max_errors
	));

	return true;
}


#ifdef USE_NVML
template<>
bool convert_driver<wtf_ptr<NvmlSensorDriver>>(const Node &node, wtf_ptr<NvmlSensorDriver> &sensor)
{
	if (!node[kw_nvidia])
		return false;

	opt<vector<int>> correction = decode_opt<vector<int>>(node[kw_correction]);
	bool optional = node[kw_optional] ? node[kw_optional].as<bool>() : false;
	opt<unsigned int> max_errors = decode_opt<unsigned int>(node[kw_max_errors]);

	sensor = wtf_ptr<NvmlSensorDriver>(new NvmlSensorDriver(node[kw_nvidia].as<string>(), optional, correction, max_errors));

	return true;
}
#endif //USE_NVML


#ifdef USE_ATASMART
template<>
bool convert_driver<wtf_ptr<AtasmartSensorDriver>>(const Node &node, wtf_ptr<AtasmartSensorDriver> &sensor)
{
	if (!node[kw_atasmart])
		return false;

	opt<vector<int>> correction = decode_opt<vector<int>>(node[kw_correction]);
	bool optional = node[kw_optional] ? node[kw_optional].as<bool>() : false;
	opt<unsigned int> max_errors = decode_opt<unsigned int>(node[kw_max_errors]);

	sensor = make_wtf<AtasmartSensorDriver>(node[kw_atasmart].as<string>(), optional, correction, max_errors);

	return true;
}
#endif //USE_ATASMART


#ifdef USE_LM_SENSORS
template<>
bool convert_driver<wtf_ptr<LMSensorsDriver>>(const Node &node, wtf_ptr<LMSensorsDriver> &sensor)
{
	if (!node[kw_chip])
		return false;

	if (!node[kw_ids]) {
		throw YamlError(get_mark_compat(node), "No temperature inputs were specified.");
	}

	string chip_name = node[kw_chip].as<string>();
	vector<string> feature_names = node[kw_ids].as<vector<string>>();

	opt<vector<int>> correction = decode_opt<vector<int>>(node[kw_correction]);
	bool optional = node[kw_optional] ? node[kw_optional].as<bool>() : false;
	opt<unsigned int> max_errors = decode_opt<unsigned int>(node[kw_max_errors]);

	if (correction && correction->size() != feature_names.size()) {
		throw YamlError(
			get_mark_compat(node[kw_ids]),
			MSG_CONF_CORRECTION_LEN(chip_name, correction->size(), feature_names.size())
		);
	}

	sensor = make_wtf<LMSensorsDriver>(chip_name, feature_names, optional, correction, max_errors);
	return true;
}
#endif // USE_LM_SENSORS


template<>
bool convert_driver<wtf_ptr<TpFanDriver>>(const Node &node, wtf_ptr<TpFanDriver> &fan)
{
	if (!node[kw_tpacpi])
		return false;

	bool optional = node[kw_optional] ? node[kw_optional].as<bool>() : false;
	opt<unsigned int> max_errors = decode_opt<unsigned int>(node[kw_max_errors]);

	fan = make_wtf<TpFanDriver>(node[kw_tpacpi].as<string>(), optional, max_errors);
	return true;
}


template<>
bool convert_driver<vector<wtf_ptr<HwmonFanDriver>>>(const Node &node, vector<wtf_ptr<HwmonFanDriver>> &fans)
{
	if (!node[kw_hwmon])
		return false;

	string path = node[kw_hwmon].as<string>();
	opt<string> name = decode_opt<string>(node[kw_name]);
	bool optional = node[kw_optional] ? node[kw_optional].as<bool>() : false;
	opt<vector<unsigned int>> indices = decode_opt<vector<unsigned int>>(node[kw_indices]);
	opt<unsigned int> max_errors = decode_opt<unsigned int>(node[kw_max_errors]);

	shared_ptr<HwmonInterface<FanDriver>> hwmon_iface = std::make_shared<HwmonInterface<FanDriver>>(
		path, name, indices
	);

	if (!indices && optional)
		throw YamlError(
			get_mark_compat(node),
			"An optional hwmon fan must have an \"indices\" entry so thinkfan knows how many temperatures to expect."
		);

	for (unsigned int i = 0; i < (indices ? indices->size() : 1); ++i)
		fans.push_back(wtf_ptr<HwmonFanDriver>(new HwmonFanDriver(hwmon_iface, optional, max_errors)));

	return true;
}



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
#ifdef USE_LM_SENSORS
			else if ((*it)[kw_chip]) {
				wtf_ptr<LMSensorsDriver> tmp = it->as<wtf_ptr<LMSensorsDriver>>();
				sensors.push_back(std::move(tmp));
			}
#endif // USE_LM_SENSORS
			else
				throw YamlError(get_mark_compat(*it), "Invalid sensor entry");
		}

		return sensors.size() > initial_size;
	}
};



void assign_fan_levels(vector<unique_ptr<StepwiseMapping>> &fan_configs, const Node &entry) {
	try {
		LevelEntry lvl_entry = entry.as<LevelEntry>();

		if (lvl_entry.fan_levels.size() != fan_configs.size())
			throw YamlError(get_mark_compat(entry),
				"Number of " + kw_speed + " entries doesn't match number of " + kw_fans
			);

		size_t fan_idx = 0;
		for (unique_ptr<StepwiseMapping> &fan_cfg : fan_configs) {
			try {
				fan_cfg->add_level(
					std::make_unique<ComplexLevel>(
						lvl_entry.fan_levels[fan_idx].first,
						lvl_entry.lower_limit,
						lvl_entry.upper_limit
					)
				);
			} catch (ConfigError &e) {
				throw YamlError(get_mark_compat(entry), e.what());
			}
			++fan_idx;
		}
	} catch (YAML::BadConversion &) {
		for (unique_ptr<StepwiseMapping> &fan_cfg : fan_configs) {
			// Have to copy the wtf_ptr first because frickin Ubuntu still haven't updated their libyaml-cpp
			wtf_ptr<SimpleLevel> l = entry.as<wtf_ptr<SimpleLevel>>();
			fan_cfg->add_level(unique_ptr<Level>(l.release()));
		}
	}
}



template<>
struct convert<vector<wtf_ptr<FanConfig>>> {
	static bool decode(const Node &fans_node, vector<wtf_ptr<FanConfig>> &fan_configs)
	{
		auto initial_size = fan_configs.size();
		vector<unique_ptr<FanDriver>> fan_drivers;
		if (!fans_node.IsSequence())
			throw YamlError(get_mark_compat(fans_node), "Fan entries must be a sequence. Forgot the dashes?");
		for (Node::const_iterator fans_it = fans_node.begin(); fans_it != fans_node.end(); ++fans_it) {
			try {
				for (auto f : fans_it->as<vector<wtf_ptr<HwmonFanDriver>>>())
					fan_drivers.push_back(unique_ptr<FanDriver>(f.release()));
			} catch (BadConversion &) {
				// Have to copy the wtf_ptr first because frickin Ubuntu still haven't updated their libyaml-cpp
				wtf_ptr<TpFanDriver> f { fans_it->as<wtf_ptr<TpFanDriver>>() };
				fan_drivers.push_back(unique_ptr<FanDriver>(f.release()));
			}

			const Node levels_node = (*fans_it)[kw_levels];
			if (levels_node) {
				if (!levels_node.IsSequence())
					throw YamlError(
						get_mark_compat(levels_node),
						"Level entries must be a sequence. Forgot the dashes?"
					);

				vector<unique_ptr<StepwiseMapping>> stepwise_mappings;

				for (unique_ptr<FanDriver> &fan_drv : fan_drivers) {
					stepwise_mappings.push_back(
						std::make_unique<StepwiseMapping>(std::move(fan_drv))
					);
				}
				fan_drivers.clear();

				for (const Node &lvl : levels_node)
					assign_fan_levels(stepwise_mappings, lvl);

				for (unique_ptr<StepwiseMapping> &mapping : stepwise_mappings)
					// Jump through ALL the Ubuntu hoops    (.............................................)
					fan_configs.push_back(wtf_ptr<FanConfig>(new unique_ptr<FanConfig>(std::move(mapping))));
			}
		}

		return fan_configs.size() > initial_size;
	}
};



vector<int> get_limit(const Node &n) {
	vector<int> rv;
	if (!n.IsSequence())
		throw YamlError(get_mark_compat(n), "Temperature limit must be a sequence");
	for (const Node &m : n) {
		try {
			int i = m.as<int>();
			if (i == numeric_limits<int>::min())
				throw YamlError(get_mark_compat(m), std::to_string(i) + " is not a valid temperature limit");
			rv.push_back(i);
		} catch (BadConversion &) {
			string s = m.as<string>();
			if (s != "_")
				throw YamlError(get_mark_compat(m), s + " is not a valid temperature limit");
			rv.push_back(std::numeric_limits<int>::max());
		}
	}
	return rv;
}



pair<string, int> get_fan_level(const Node &n) {
	int level_n;
	string level_s;
	try {
		level_n = n.as<int>();
		if (level_n == 127)
			level_s = "level disengaged";
		else
			level_s = "level " + std::to_string(level_n);
	} catch (BadConversion &) {
		level_s = n.as<string>();
		level_n = Level::string_to_int(level_s);
	}
	return {level_s, level_n};
}



template<>
struct convert<LevelEntry> {
	static bool decode(const Node &node, LevelEntry &rv)
	{
		const Node &n_lower = node[kw_lower], &n_upper = node[kw_upper];
		if (!(node[kw_speed] && (n_lower || n_upper)))
			return false;

		if (node[kw_speed].IsSequence()) {
			for (const Node &n_lvl : node[kw_speed])
				rv.fan_levels.push_back(get_fan_level(n_lvl));
		}
		else
			rv.fan_levels.push_back(get_fan_level(node[kw_speed]));

		if (n_lower)
			rv.lower_limit = get_limit(n_lower);
		if (n_upper)
			rv.upper_limit = get_limit(n_upper);

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

	for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
		const string key = it->first.as<string>();

		if (key != kw_sensors && key != kw_fans && key != kw_levels)
			throw YamlError(get_mark_compat(it->first), "Unknown keyword");
	}

	if (node[kw_sensors]) {
		for (auto s : node[kw_sensors].as<vector<wtf_ptr<SensorDriver>>>())
			config->add_sensor(unique_ptr<SensorDriver>(s.release()));
	}
	else
		throw YamlError(get_mark_compat(node), "Missing \"sensors:\" entry");

	if (node[kw_fans]) {
		try {
			// Each fan with its own levels section (supports multiple fans)
			for (auto fan_cfg : node[kw_fans].as<vector<wtf_ptr<FanConfig>>>()) {
				// Have to copy the wtf_ptr first because frickin Ubuntu still haven't updated their libyaml-cpp
				wtf_ptr<FanConfig> fu { fan_cfg }; // It's const on feckin Ubuntu
				config->add_fan_config(unique_ptr<FanConfig>(fu.release()));
			}
		} catch (BadConversion &) {
			// Single fan entry with separate levels section below.
			vector<unique_ptr<FanDriver>> fans;

			if (node[kw_fans].size() > 1)
				throw YamlError(get_mark_compat(node[kw_fans]), "When multiple fans are configured, each must have its own 'levels:' section");
			try {
				wtf_ptr<TpFanDriver> fu { node[kw_fans][0].as<wtf_ptr<TpFanDriver>>() };
				fans.push_back(unique_ptr<FanDriver>(fu.release()));
			} catch (BadConversion &) {
				for (auto &fan_drv : node[kw_fans][0].as<vector<wtf_ptr<HwmonFanDriver>>>()) {
					wtf_ptr<HwmonFanDriver> fu { fan_drv };
					fans.push_back(unique_ptr<FanDriver>(fu.release()));
				}
			}

			vector<unique_ptr<StepwiseMapping>> fan_configs;

			for (unique_ptr<FanDriver> &fan : fans)
				fan_configs.push_back(std::make_unique<StepwiseMapping>(std::move(fan)));
			fans.clear();

			if (node[kw_levels]) {
				// Separate "levels:" section
				if (config->fan_configs().size())
					throw YamlError(get_mark_compat(node), "Cannot have a separate 'levels:' section when some fan already has specific levels assigned");
				if (!node[kw_levels].IsSequence())
					throw YamlError(get_mark_compat(node), "Level entries must be a sequence. Forgot the dashes?");


				for (const Node &n_lvl : node[kw_levels])
					assign_fan_levels(fan_configs, n_lvl);

				for (unique_ptr<StepwiseMapping> &fan_cfg : fan_configs)
					config->add_fan_config(std::move(fan_cfg));
			}
			else
				throw YamlError(get_mark_compat(node), "Missing \"levels:\" entry");
		}

	}
	else
		throw YamlError(get_mark_compat(node), "Missing \"fans:\" entry");

	return true;
}



}



