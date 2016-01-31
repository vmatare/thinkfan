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

// Work around GCC bug #66145 (will hopefully be fixed in 5.4)
// This needs to be defined before library headers are included.
// cf. https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_dual_abi.html
// and https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66145
#if __GNUC__ >= 5 && __GNUC_MINOR__ < 4
#define _GLIBCXX_USE_CXX11_ABI 0
#endif

#include <exception>
#include <string>

#include "message.h"
#include "thinkfan.h"

#ifndef MAX_BACKTRACE_DEPTH
#define MAX_BACKTRACE_DEPTH 64
#endif

namespace thinkfan {

class Error : public std::exception {
protected:
	string msg_;
	string backtrace_;
public:
	Error(const string &message = "");

	virtual const char* what() const _GLIBCXX_USE_NOEXCEPT override;
	const string &backtrace() const;
};


class Bug : public Error {
public:
	Bug(const string &desc = "");
};

class ExpectedError : public Error {
	using Error::Error;
};

class SyntaxError : public ExpectedError {
public:
	SyntaxError(const string filename, const size_t offset, const string &input);
};


class ConfigError : public ExpectedError {
public:
	ConfigError(const string &reason);
};


class ParserMisdefinition : public ExpectedError {};
class ParserOOM : public ExpectedError {};
class MixedLevelSpecs : public ExpectedError {};
class LimitLengthMismatch : public ExpectedError {};


class SystemError : public ExpectedError {
public:
	SystemError(const string &reason)
	: ExpectedError(reason) {}
};

class InvocationError : public ExpectedError {
public:
	InvocationError(const string &message);
};

void handle_uncaught();


}

#endif /* THINKFAN_ERROR_H_ */
