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
#include "wtf_ptr.h"

namespace thinkfan {


vector<string> find_hwmons_by_name(const string &path, const string &name);


template<class HwmonT>
vector<string> find_hwmons_by_indices(const string &path, const vector<unsigned int> &indices);


class HwmonInterface {
public:
	HwmonInterface();
	HwmonInterface(const string &base_path, opt<const string> &&name, opt<unsigned int> &&index);

	template<class HwmonT>
	string lookup();

private:
	static string find_hwmon_by_name(const string &path, const string &name);

	template<class HwmonT>
	static string find_hwmon_by_index(const string &path, unsigned int index);

protected:
	opt<const string> base_path_;
	opt<const string> name_;
	opt<unsigned int> index_;
};


} // namespace thinkfan
