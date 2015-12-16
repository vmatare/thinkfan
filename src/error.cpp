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
#include <execinfo.h>
#include <cstring>

namespace thinkfan {


static string make_backtrace()
{
	string backtrace_;
	void *bt_buffer[MAX_BACKTRACE_DEPTH];
	int stack_depth = ::backtrace(bt_buffer, MAX_BACKTRACE_DEPTH);
	if (stack_depth == MAX_BACKTRACE_DEPTH)
		log(TF_ERR) << "Max backtrace depth reached. Backtrace may be incomplete." << flush;

	char **bt_pretty = backtrace_symbols(bt_buffer, stack_depth);
	for (int i=0; i < stack_depth; ++i) {
		backtrace_ += bt_pretty[i];
		backtrace_ += '\n';
	}
	free(bt_pretty);
	return backtrace_;
}


Error::Error(const string &message)
: msg_(message), backtrace_(make_backtrace())
{}


const string &Error::backtrace() const
{ return backtrace_; }


const char* Error::what() const _GLIBCXX_USE_NOEXCEPT
{ return msg_.c_str(); }


Bug::Bug(const string &desc)
: Error(desc)
{}


void handle_uncaught()
{
	std::string err = std::strerror(errno);
	try {
		std::rethrow_exception(std::current_exception());
	} catch (const std::exception &e) {
		log(TF_ERR) << "Unhandled exception: " << e.what() << ". errno = " << err << "." << flush <<
				"Backtrace:" << make_backtrace() << flush <<
				MSG_BUG << flush;
	}
}


SyntaxError::SyntaxError(const string filename, const size_t offset, const string &input)
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
	msg_ += std::to_string(line) + ": Syntax error:\n";
	std::string::size_type line_end = input.find('\n', line_start) - line_start;
	msg_ += input.substr(line_start, line_end) + '\n';
	msg_ += std::string(offset - line_start, ' ') + "^\n";
}


ConfigError::ConfigError(const std::string &reason)
: ExpectedError("Invalid config: " + reason) {}


InvocationError::InvocationError(const std::string &message)
: ExpectedError("Invalid command line: " + message) {}


}
