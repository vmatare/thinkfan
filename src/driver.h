#pragma once

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

#include "thinkfan.h"
#include "error.h"
#include <functional>
#include <optional>

namespace thinkfan {


class Driver {
protected:
	Driver(bool optional, unsigned int max_errors);

	template<typename FnSignatureT>
	using FN = std::function<FnSignatureT>;

public:
	void try_init();
	unsigned int errors() const;
	unsigned int max_errors() const;
	virtual bool optional() const;

	/** @return The identifier returned by @a lookup(). Calling this method before @a lookup() has completed
	 *  will result in an exception. */
	const string &path() const;

	void robust_op(FN<void ()> op_fn, FN<void (const ExpectedError &)> skip_fn);

	template<class DriverT, typename... ArgTs>
	void robust_io(void (DriverT::*io_func)(ArgTs...), ArgTs &&... args);

	bool initialized() const;
	bool available() const;

private:
	unsigned int max_errors_;
	unsigned int errors_;
	bool optional_;
	bool initialized_;

	void handle_io_error_(const ExpectedError &e, FN<void (const ExpectedError &)> skip_fn);

protected:
	virtual void init() = 0;

	/** @brief Identify the hardware resource to be associated with this config entry
	 *  @return A string that represents a valid identifier (e.g. a hwmon path) for the driver IFF
	 *  the driver's resource has been found and is ready to be initialized. Otherwise, throw an instance of
	 *  @a thinkfan::ExpectedError.
	 *  The string returned by this will be accessible via the @a path() method. */
	virtual string lookup() = 0;

	/// @return A user-friendly name for the type of driver represented by the implementor
	virtual string type_name() const = 0;

	virtual void skip_io_error(const ExpectedError &);

	opt<const string> path_;
};


template<class DriverT, typename... ArgTs>
void Driver::robust_io(void (DriverT::*io_func)(ArgTs...), ArgTs &&... args)
{
	using namespace std::placeholders;

	if (!available() || !initialized())
		try_init();

	if (initialized())
		robust_op(
			[&] () {
				(static_cast<DriverT *>(this)->*io_func)(
					std::forward<ArgTs>(args)...
				);
			},
			std::bind(&Driver::skip_io_error, this, _1)
		);
}




} // namespace thinkfan
