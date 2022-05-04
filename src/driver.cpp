/********************************************************************
 * driver.h: Common functionality for all drivers
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

#include "driver.h"

namespace thinkfan {

Driver::Driver(bool optional, unsigned int max_errors)
: max_errors_(max_errors)
, errors_(0)
, optional_(optional)
, initialized_(false)
{}


void Driver::try_init()
{
	robust_op(
		[&] () {
			if (!available())
				path_.emplace(lookup());
			init();
			initialized_ = true;
		},
		[&] (const ExpectedError &e) { /* skip_fn */
			if (!optional())
				log(TF_WRN)
					<< "Error " << errors() << "/" << max_errors()
					<< " while initializing driver: " << e.what() << flush
				;
		}
	);
}


unsigned int Driver::errors() const
{ return errors_; }

unsigned int Driver::max_errors() const
{ return std::max(max_errors_, static_cast<unsigned int>(tolerate_errors)); }

bool Driver::optional() const
{ return optional_; }

const string &Driver::path() const
{ return path_.value(); }

bool Driver::initialized() const
{ return initialized_; }

bool Driver::available() const
{ return path_.has_value(); }

void Driver::skip_io_error(const ExpectedError &e)
{ log(TF_ERR) << e.what() << flush; }



} // namespace thinkfan
