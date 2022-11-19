#pragma once

/********************************************************************
 * hwmon.h: Helper functionality for sysfs hwmon interface
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

#include "thinkfan.h"

#include <dirent.h>

namespace thinkfan {


class HwmonSensorDriver;
class HwmonFanDriver;


template<class HwmonT>
class HwmonInterface {
public:
	HwmonInterface();
	HwmonInterface(const string &base_path, opt<const string> name, opt<vector<unsigned int>> indices);

	string lookup();

private:
	static vector<string> find_files(const string &path, const vector<unsigned int> &indices);
	static string filename(int index);

	static vector<string> find_hwmons_by_name(const string &path, const string &name, unsigned char depth);
	static vector<string> find_hwmons_by_indices(const string &path, const vector<unsigned int> &indices, unsigned char depth);

protected:
	opt<const string> base_path_;
	opt<const string> name_;
	opt<vector<unsigned int>> indices_;
	vector<string> found_paths_;
	opt<vector<string>::const_iterator> paths_it_;
};


} // namespace thinkfan
