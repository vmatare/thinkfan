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
#include "message.h"

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
		[&]/* op_fn */() {
			if (!available())
				path_.emplace(lookup());
			init();
			initialized_ = true;
		},
		[&]/* skip_fn */(const ExpectedError &e) {
			log(optional() ? TF_DBG : TF_INF) << "Ignoring error ";
			if (max_errors() && !optional())
				log() << errors() << "/" << max_errors() << " ";
			log() << "while initializing " << type_name() << ": " << e.what() << flush;
		}
	);
}


void Driver::robust_op(FN<void ()> op_fn, FN<void (const ExpectedError &)> skip_fn)
{
	try {
		errors_++;
		op_fn();
		errors_ = 0;
	} catch (DriverInitError &e) {
		e.set_context(type_name());
		handle_io_error_(e, skip_fn);
	} catch (SystemError &e) {
		handle_io_error_(e, skip_fn);
	} catch (IOerror &e) {
		handle_io_error_(e, skip_fn);
	} catch (std::ios_base::failure &e) {
		handle_io_error_(IOerror(e.what(), THINKFAN_IO_ERROR_CODE(e)), skip_fn);
	}
}


void Driver::handle_io_error_(const ExpectedError &e, FN<void (const ExpectedError &)> skip_fn)
{
	if (optional() || tolerate_errors || errors() < max_errors() || !chk_sanity)
		skip_fn(e);
	else
		throw e;
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
