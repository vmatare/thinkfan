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
#include "wtf_ptr.h"
#include "error.h"

#include <dirent.h>
#include <fnmatch.h>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <algorithm>
#include <functional>

namespace thinkfan {


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


static vector<unsigned int> filter_indices;

static int file_filter(const struct dirent *entry, const string &pfx, const string &sfx)
{
	if (entry->d_type == DT_REG) {
		int idx = get_index(entry->d_name, pfx, sfx);
		if (
			idx >= 0
			&& std::find(
				filter_indices.begin(), filter_indices.end(), idx
			) != filter_indices.end()
		)
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


static vector<string> find_hwmons_by_name_(
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

		auto found = find_hwmons_by_name_(subdir, name, depth + 1);
		result.insert(result.end(), found.begin(), found.end());
	}
	free(entries);

	return result;
}

vector<string> find_hwmons_by_name(const string &path, const string &name)
{ return find_hwmons_by_name_(path, name, 1); }



template<class T>
string hwmon_filename(int index);

template<>
string hwmon_filename<HwmonSensorDriver>(int index)
{ return "temp" + std::to_string(index) + "_input"; }

template<>
string hwmon_filename<HwmonFanDriver>(int index)
{ return "pwm" + std::to_string(index); }


template<class HwmonT>
static vector<string> find_hwmons_by_indices_(const string &path, const vector<unsigned int> &indices, unsigned char depth)
{
	vector<string> rv;
#include <functional>

	const unsigned char max_depth = 3;

	filter_indices = indices;

	while (depth <= max_depth) {
		struct dirent **entries;
		int nentries = scandir<HwmonT>(path, &entries);

		if (nentries < 0)
			throw IOerror("Error scanning " + path + ": ", errno);

		int temp_idx = -1;

		if (nentries > 0) {
			for (int i = 0; i < nentries; i++) {
				temp_idx = get_index<HwmonT>(entries[i]->d_name);
				if (temp_idx < 0)
					break; // no index found in file name

				auto it = std::find(filter_indices.begin(), filter_indices.end(), temp_idx);

				rv.push_back(path + "/" + entries[i]->d_name);
				filter_indices.erase(it);
				// stop crawling at this level
				depth = std::numeric_limits<unsigned char>::max();
			}
			if (!filter_indices.empty()) {
				string list;
				for (int i : filter_indices)
					list += hwmon_filename<HwmonT>(i) + ", ";
				list = list.substr(0, list.length() - 2);
				throw DriverInitError("Could not find the following files at " + path + ": " + list);
			}
		}

		if (nentries == 0 || temp_idx < 0) {
			nentries = ::scandir(path.c_str(), &entries, filter_hwmon_dirs, alphasort);
			if (nentries < 0)
				throw IOerror("Error scanning " + path + ": ", errno);

			if (nentries > 0 && depth <= max_depth) {
				for (int i = 0; i < nentries && rv.empty(); i++)
					rv = find_hwmons_by_indices_<HwmonT>(
						path + "/" + entries[i]->d_name,
						indices,
						depth + 1
					);
			}
			else
				throw DriverInitError("Could not find an `hwmon*' directory or `temp*_input' file in " + path + ".");
		}

		for (int i = 0; i < nentries; i++)
			free(entries[i]);
		free(entries);
	}

	return rv;
}

template<class HwmonT>
vector<string> find_hwmons_by_indices(const string &path, const vector<unsigned int> &indices)
{ return find_hwmons_by_indices_<HwmonT>(path, indices, 0); }




HwmonInterface::HwmonInterface()
{}

HwmonInterface::HwmonInterface(const string &base_path, opt<const string> name, opt<unsigned int> index)
: base_path_(base_path)
, name_(name)
, index_(index)
{}


template<class HwmonT>
string HwmonInterface::lookup()
{
	if (!base_path_)
		throw Bug("Can't lookup sensor because it has no base path");

	string path = base_path_.value();

	if (name_)
		path = find_hwmon_by_name(path, name_.value());
	if (index_)
		path = find_hwmon_by_index<HwmonT>(path, index_.value());

	base_path_.reset();
	name_.reset();
	index_.reset();

	return path;
}

template string HwmonInterface::lookup<HwmonFanDriver>();
template string HwmonInterface::lookup<HwmonSensorDriver>();


template<class Arg2T>
static inline string pick_first(std::function<vector<string>(const string &, Arg2T)> fn, const string &path, Arg2T &&arg2)
{
	vector<string> paths = fn(path, arg2);
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
		throw DriverInitError(path + ": " + msg);
	}
	return paths[0];
}


string HwmonInterface::find_hwmon_by_name(const string &path, const string &name)
{ return pick_first(std::function(find_hwmons_by_name), path, name); }

template<class HwmonT>
string HwmonInterface::find_hwmon_by_index(const string &path, unsigned int index)
{ return pick_first(std::function(find_hwmons_by_indices<HwmonT>), path, { index }); }





} // namespace thinkfan
