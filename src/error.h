/********************************************************************
 * error.h: Custom exceptions
 * (C) 2015, Victor Mataré
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

#ifndef THINKFAN_ERROR_H_
#define THINKFAN_ERROR_H_

#include <exception>
#include <string>
#include "message.h"
#include "thinkfan.h"

namespace thinkfan {


class Error : public std::exception {
protected:
	string msg_;
public:
	Error(const string &message = "");

	virtual const char* what() const _GLIBCXX_USE_NOEXCEPT override;
};


class SyntaxError : public Error {
public:
	SyntaxError(const string filename, const size_t offset, const string &input);
};


class ConfigError : public Error {
public:
	ConfigError(const string &reason);
};


class SystemError : public Error {
public:
	SystemError(const string &reason)
	: Error(reason) {}
};
class DriverError : public Error {};

class InvocationError : public Error {
public:
	InvocationError(const string &message);
};



}

#endif /* THINKFAN_ERROR_H_ */
