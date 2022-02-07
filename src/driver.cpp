/********************************************************************
 * config.h: Config data structures and consistency checking.
 * (C) 2015, Victor Mataré
 *     2021, Koutheir Attouchi
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

#include "driver.h"

namespace thinkfan {

Driver::Driver(unsigned int max_errors, const string &path, bool optional)
: max_errors_(max_errors)
, errors_(0)
, optional_(optional)
, initialized_(false)
, path_(path)
{}

void Driver::try_init()
{
	try {
		init();
		initialized_ = true;
		errors_ = 0;
		return;
	} catch (std::exception &e) {
		if (++errors_ > max_errors_)
			throw e;
	}
}

unsigned int Driver::errors() const
{ return errors_; }

unsigned int Driver::max_errors() const
{ return max_errors_; }

bool Driver::optional() const
{ return optional_; }

//const string &Driver::path() const
//{ return path_; }

void Driver::set_path(const string &path)
{ path_ = path; }

bool Driver::initialized() const
{ return initialized_; }


void Driver::handle_io_error_(const ExpectedError &e)
{
	if (!(optional() || tolerate_errors || errors() < max_errors()))
		error<SensorLost>(e);
	skip_io_error(e);
}

void Driver::skip_io_error(const ExpectedError &e)
{ log(TF_ERR) << e.what() << flush; }


} // namespace thinkfan
