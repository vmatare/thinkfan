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
#include "drivers.h"

#ifdef USE_YAML
#include <yaml-cpp/exceptions.h>
#include <yaml-cpp/node/node.h>
#endif

#ifndef MAX_BACKTRACE_DEPTH
#define MAX_BACKTRACE_DEPTH 64
#endif

namespace thinkfan {


std::string demangle(const char* name);
// Cf. http://stackoverflow.com/questions/281818/unmangling-the-result-of-stdtype-infoname
template <class T>
std::string type(const T &t)
{ return demangle(typeid(t).name()); }


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

class IOerror : public ExpectedError {
private:
	const int code_;
public:
	IOerror(const string &message, const int error_code);
	int code();
};


class SensorLost : public ExpectedError {
public:
	template<class CauseT>
	SensorLost(const CauseT &cause) {
		msg_ = string("Lost sensor ") + cause.what();
	}
};


class SyntaxError : public ExpectedError {
public:
	SyntaxError(const string filename, const std::ptrdiff_t offset, const string &input);
};


#ifdef USE_YAML

class YamlError : public Error {
public:
	YamlError(const YAML::Mark &mark = YAML::Mark::null_mark(), const string &msg = "");
	YAML::Mark mark;
};

class MissingEntry : public YamlError {
public:
	MissingEntry(const string &entry);
};

#endif


class ConfigError : public ExpectedError {
public:
	ConfigError(const string &reason);
#ifdef USE_YAML
	ConfigError(const string &filename, const YAML::Mark &mark, const string &input, const string &msg);
#endif
};


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
