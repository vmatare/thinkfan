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

namespace thinkfan {

Error::Error(const std::string &message)
: msg_(message)
{}


const char* Error::what() const _GLIBCXX_USE_NOEXCEPT
{ return msg_.c_str(); }


SyntaxError::SyntaxError(const std::string filename, const size_t offset, const std::string &input)
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
	std::string::size_type len_tmp = msg_.length();
	std::string::size_type line_end = input.find('\n', line_start) - line_start;
	msg_ += input.substr(line_start, line_end) + '\n';
	msg_ += std::string(offset - line_start, ' ') + "^\n";
}


ConfigError::ConfigError(const std::string &reason)
: Error("Invalid config: " + reason) {}


InvocationError::InvocationError(const std::string &message)
: Error("Invalid command line: " + message) {}


}
