/********************************************************************
 * hwmon.cpp: Helper functionality for sysfs hwmon interface
 * (C) 2022, Victor Mataré
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

#include "hwmon.h"
#include "message.h"
#include "error.h"

#include <fnmatch.h>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <filesystem>

namespace thinkfan {

namespace filesystem = std::filesystem;


static int filter_hwmon_dirs(const struct dirent *entry)
{
	return (entry->d_type == DT_DIR || entry->d_type == DT_LNK)
		&& (string(entry->d_name) == "hwmon" || string(entry->d_name) == "device");
}


static int filter_subdirs(const struct dirent *entry)
{
	return (entry->d_type & DT_DIR || entry->d_type == DT_LNK)
		&& string(entry->d_name) != "." && string(entry->d_name) != ".."
		&& string(entry->d_name) != "subsystem";
}


template<>
int HwmonInterface<SensorDriver>::filter_driver_file(const struct dirent *entry)
{
	int idx;
	return (entry->d_type == DT_REG || entry->d_type == DT_LNK)
		&& !::sscanf(entry->d_name, "temp%d_input", &idx)
	;
}

template<>
int HwmonInterface<FanDriver>::filter_driver_file(const struct dirent *entry)
{
	int idx;
	return (entry->d_type == DT_REG || entry->d_type == DT_LNK)
		&& !::sscanf(entry->d_name, "pwm%d", &idx)
	;
}


template<int (* filter_fn)(const struct dirent *)>
vector<filesystem::path> dir_entries(const filesystem::path &dir)
{
	struct dirent **entries;
	int nentries = ::scandir(dir.c_str(), &entries, filter_fn, nullptr);
	if (nentries == -1)
		return {};

	vector<filesystem::path> rv;
	for (int i = 0; i < nentries; ++i) {
		rv.emplace_back(dir / entries[i]->d_name);
		::free(entries[i]);
	}
	::free(entries);
	return rv;
}


template<class HwmonT>
vector<string> HwmonInterface<HwmonT>::find_files(const string &path, const vector<unsigned int> &indices)
{
	vector<string> rv;
	for (unsigned int idx : indices) {
		const string fpath(path + "/" + filename(idx));
		std::ifstream f(fpath);
		if (f.is_open() && f.good())
			rv.push_back(fpath);
		else
			throw IOerror("Can't find hwmon file: " + fpath, errno);
	}
	return rv;
}

template<>
string HwmonInterface<SensorDriver>::filename(unsigned int index)
{ return "temp" + std::to_string(index) + "_input"; }

template<>
string HwmonInterface<FanDriver>::filename(unsigned int index)
{ return "pwm" + std::to_string(index); }



template<class HwmonT>
HwmonInterface<HwmonT>::HwmonInterface()
{}

template<class HwmonT>
HwmonInterface<HwmonT>::HwmonInterface(const string &base_path, opt<const string> name, opt<const string> model, opt<vector<unsigned int>> indices)
: base_path_(base_path)
, name_(name)
, model_(model)
, indices_(indices)
{}


template<class HwmonT>
vector<string> HwmonInterface<HwmonT>::find_hwmons_by_name(
	const string &path,
	const string &name,
	unsigned char depth
) {
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

	for (const filesystem::path &subdir : dir_entries<filter_subdirs>(path)) {
		struct stat statbuf;
		int err = stat(path.c_str(), &statbuf);
		if (err || (statbuf.st_mode & S_IFMT) != S_IFDIR)
			continue;

		auto found = find_hwmons_by_name(subdir, name, depth + 1);
		result.insert(result.end(), found.begin(), found.end());
	}

	return result;
}


template<class HwmonT>
vector<string> HwmonInterface<HwmonT>::find_hwmons_by_model(
	const string &path,
	const string &model,
	unsigned char depth
) {
	const unsigned char max_depth = 5;
	vector<string> result;

	ifstream f(path + "/model");
	if (f.is_open() && f.good()) {
		string tmp;
		if (getline(f, tmp)) {
			tmp = tmp.erase(tmp.find_last_not_of(" \t\n\r\f\v") + 1);
			if (tmp == model) {
				result.push_back(path);
				return result;
			}
		}
	}
	if (depth >= max_depth) {
		return result; // don't recurse to subdirs
	}

	for (const filesystem::path &subdir : dir_entries<filter_subdirs>(path)) {
		struct stat statbuf;
		int err = stat(path.c_str(), &statbuf);
		if (err || (statbuf.st_mode & S_IFMT) != S_IFDIR)
			continue;

		auto found = find_hwmons_by_model(subdir, model, depth + 1);
		result.insert(result.end(), found.begin(), found.end());
	}

	return result;
}


template<class HwmonT>
vector<string> HwmonInterface<HwmonT>::find_hwmons_by_indices(
	const string &path,
	const vector<unsigned int> &indices,
	unsigned char depth
) {
	constexpr unsigned char max_depth = 3;

	try {
		return find_files(path, indices);
	}
	catch (IOerror &) {
		if (depth <= max_depth) {
			vector<filesystem::path> hwmon_dirs = dir_entries<filter_hwmon_dirs>(path);
			if (hwmon_dirs.empty())
				throw IOerror("Error scanning " + path + ": ", errno);

			vector<string> rv;
			for (const filesystem::path &hwmon_dir : hwmon_dirs) {
				rv = HwmonInterface<HwmonT>::find_hwmons_by_indices(
					hwmon_dir,
					indices,
					depth + 1
				);
				if (rv.size())
					break;
			}

			return rv;
		}
		else
			throw DriverInitError("Could not find an `hwmon*' directory or `temp*_input' file in " + path + ".");
	}
}


template<class HwmonT>
string HwmonInterface<HwmonT>::lookup()
{
	if (!paths_it_) {
		if (!base_path_)
			throw Bug("Can't lookup sensor because it has no base path");

		string path = *base_path_;

		if (name_) {
			vector<string> paths = find_hwmons_by_name(path, name_.value(), 1);
			if (paths.size() != 1) {
				string msg(path + ": ");
				if (paths.size() == 0) {
					msg += "Could not find an hwmon with this name: " + name_.value();
				} else {
					msg += MSG_MULTIPLE_HWMONS_FOUND;
					for (string hwmon_path : paths)
						msg += " " + hwmon_path;
				}
				throw DriverInitError(msg);
			}
			path = paths[0];
		}
		if (model_) {
			vector<string> paths = find_hwmons_by_model(path, model_.value(), 1);
			if (paths.size() != 1) {
				string msg(path + ": ");
				if (paths.size() == 0) {
					msg += "Could not find a hwmon with this model: " + model_.value();
				} else {
					msg += MSG_MULTIPLE_HWMONS_FOUND;
					for (string hwmon_path : paths)
						msg += " " + hwmon_path;
				}
				throw DriverInitError(msg);
			}
			path = paths[0];
		}
		if (indices_) {
			found_paths_ = find_hwmons_by_indices(path, indices_.value(), 0);
			if (found_paths_.size() == 0)
				throw DriverInitError(path + ": " + "Could not find any hwmons in " + path);
		}
		else {
			vector<filesystem::path> paths = dir_entries<filter_driver_file>(path);
			found_paths_.assign(paths.begin(), paths.end());
		}

		paths_it_.emplace(found_paths_.begin());
	}

	if (*paths_it_ >= found_paths_.end())
		throw Bug(string(__func__) + ": found_paths_ iterator out of bounds");

	return *paths_it_.value()++;
}



template class HwmonInterface<FanDriver>;
template class HwmonInterface<SensorDriver>;





} // namespace thinkfan
