#pragma once

/********************************************************************
 * driver.h: Generic
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
#include "error.h"

namespace thinkfan {


class Driver {
protected:
	Driver(unsigned int max_errors, const string &path, bool optional);

public:
	void try_init();
	unsigned int errors() const;
	unsigned int max_errors() const;
	virtual bool optional() const;
	const string &path() const;
	void set_path(const string &path);

	template<class DriverT, typename... ArgTs>
	void robust_io(void (DriverT::*f)(ArgTs...), ArgTs &&... args);

	bool initialized() const;

private:
	const unsigned int max_errors_;
	unsigned int errors_;
	const bool optional_;
	bool initialized_;
	string path_;

	void handle_io_error_(const ExpectedError &e);

protected:
	virtual void init() = 0;
	virtual void skip_io_error(const ExpectedError &);
};


template<class DriverT, typename... ArgTs>
void Driver::robust_io(void (DriverT::*io_func)(ArgTs...), ArgTs &&... args)
{
	try {
		errors_++;

		if (!initialized_)
			init();

		(static_cast<DriverT *>(this)->*io_func)(
			std::forward<ArgTs>(args)...
		);

		errors_ = 0;
	} catch (SystemError &e) {
		handle_io_error_(e);
	} catch (IOerror &e) {
		handle_io_error_(e);
	} catch (std::ios_base::failure &e) {
		handle_io_error_(IOerror(e.what(), THINKFAN_IO_ERROR_CODE(e)));
	}
}



} // namespace thinkfan
