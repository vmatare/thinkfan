/********************************************************************
 * error.cpp: Custom exceptions
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

#include "error.h"
#include "message.h"

#include <features.h>
#if defined(__GLIBC__)
#include <execinfo.h>
#endif

#include <array>
#include <cstring>
#include <sstream>

#ifdef __GNUG__
#include <cstdlib>
#include <memory>
#include <cxxabi.h>
#endif

namespace thinkfan {


static string make_backtrace()
{
#if defined(__GLIBC__)
	string backtrace_;
	void *bt_buffer[MAX_BACKTRACE_DEPTH];
	int stack_depth = ::backtrace(bt_buffer, MAX_BACKTRACE_DEPTH);
	if (stack_depth == MAX_BACKTRACE_DEPTH)
		log(TF_ERR) << "Max backtrace depth reached. Backtrace may be incomplete." << flush;

	char **bt_pretty = backtrace_symbols(bt_buffer, stack_depth);
	for (int i=0; i < stack_depth; ++i) {
		string line(bt_pretty[i]);
		string::size_type i1 = line.rfind('(');
		string::size_type i2 = line.rfind('+');
		if (i1 != string::npos && i2 != string::npos && i2 - i1 > 1
		        && line[i1+1] != '+') // line contains a function name to demangle
			line = line.substr(0, i1+1) + demangle(line.substr(i1+1, i2-i1-1).c_str()) + line.substr(i2);
		backtrace_ += line;
		backtrace_ += '\n';
	}
	free(bt_pretty);
	return backtrace_;
#else
	return "[not supported by C library]";
#endif
}


#ifdef __GNUG__
// Cf. http://stackoverflow.com/questions/281818/unmangling-the-result-of-stdtype-infoname
std::string demangle(const char* name) {

	int status = -4; // some arbitrary value to eliminate the compiler warning

	std::unique_ptr<char, void(*)(void*)> res {
		abi::__cxa_demangle(name, nullptr, nullptr, &status),
		std::free
	};

	return (status==0) ? res.get() : name ;
}

#else

// does nothing if not g++
std::string demangle(const char* name) {
	return name;
}

#endif


Error::Error(const string &message)
: msg_(message),
  backtrace_(make_backtrace())
{}


const string &Error::backtrace() const
{ return backtrace_; }


const char* Error::what() const noexcept
{ return msg_.c_str(); }


IOerror::IOerror(const string &message, const int error_code)
: ExpectedError(message + std::strerror(error_code)),
  code_(error_code)
{}


int IOerror::code()
{ return code_; }


Bug::Bug(const string &desc)
: Error(desc)
{}


void handle_uncaught()
{
	std::string err = std::strerror(errno);
	try {
		std::rethrow_exception(std::current_exception());
	} catch (const std::exception &e) {
		log(TF_ERR) << "Unhandled " << demangle(typeid(e).name()) << ": "
		               << e.what() << "." << flush
		               << "errno = " << err << "." << flush
		               << flush << "Backtrace:" << flush
		               << make_backtrace() << flush
#if not defined(DISABLE_EXCEPTION_CATCHING)
		               << MSG_BUG << flush
#endif
		               ;
	}
	// We can expect to be killed by SIGABRT after this function returns.
	PidFileHolder::cleanup();
}


SyntaxError::SyntaxError(const string filename, const std::ptrdiff_t offset, const string &input)
{
	unsigned int line = 1;
	msg_ += filename + ":";
	std::string::size_type line_start = 0;
	unsigned int i = 0;
	for (i = 0; i < offset; ++i) {
		if (input[i] == '\n') {
			++line;
			line_start = i + 1;
		}
	}
	msg_ += std::to_string(line) + ": Invalid syntax:\n";
	std::string::size_type line_end = input.find('\n', line_start) - line_start;
	msg_ += input.substr(line_start, line_end) + '\n';
	msg_ += std::string(offset - line_start, ' ') + "^\n";
}


ConfigError::ConfigError(const string &reason)
: ExpectedError(reason)
, reason_(reason)
{}

void ConfigError::set_filename(const string &filename)
{ msg_ = filename + ":\n" + msg_; }

const string &ConfigError::filename() const
{ return filename_; }

const string &ConfigError::reason() const
{ return reason_; }

const char* ConfigError::what() const noexcept
{ return msg_.c_str(); }


#ifdef USE_YAML

ConfigError::ConfigError(const string &filename, const YAML::Mark &mark, const string &input, const string &msg)
: reason_(msg)
, filename_(filename)
{
	msg_ = filename + ":";
	// Another workaround for Ubuntu's libyaml-cpp0.5v5
	if (mark.pos == -1 && mark.line == -1 && mark.column == -1) {
		msg_ += string(" ") + msg + ".";
#ifdef HAVE_OLD_YAMLCPP
		msg_+= "\nYou have an ancient libyaml-cpp which can't give line numbers on errors. Please complain to your Linux distribution.";
#endif
	}
	else {
		msg_ += std::to_string(mark.line + 1) + ":\n";
		std::stringstream s(input);
		std::array<char, 1024> line;
		for (int i = 0; i <= mark.line; i++)
			s.getline(&line[0], 1024);

		msg_ += line.data() + string("\n");
		msg_ += string(static_cast<size_t>(mark.column), ' ') + "^\n";
		msg_ +=  msg + ".";
	}
}

YamlError::YamlError(const YAML::Mark &mark, const string &msg)
: Error(msg),
  mark(mark)
{}


MissingEntry::MissingEntry(const string &entry)
	: YamlError(YAML::Mark::null_mark(), string("Missing `") + entry + "' entry.")
{}

#endif


InvocationError::InvocationError(const string &message)
: ExpectedError("Invalid command line: " + message) {}


}
