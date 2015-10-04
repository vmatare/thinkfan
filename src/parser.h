/********************************************************************
 * parser.h: Recursive descent parser for the config.
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

#ifndef THINKFAN_PARSER_H_
#define THINKFAN_PARSER_H_

#include <regex.h>
#include <vector>
#include <string>
#include <iterator>
#include <exception>
#include <string.h>
#include "drivers.h"

namespace thinkfan {

using namespace std;

class ParserMisdefinition : public std::exception {};
class ParserOOM : public std::exception {};
class MixedLevelSpecs : public std::exception {};
class LimitLengthMismatch : public std::exception {};

class SimpleLevel;
class ComplexLevel;
class Config;
static const string tpacpi_path = "/proc/acpi/ibm";

template<typename ResultT>
class Parser {
public:
	typedef ResultT* ResultT_;

	Parser() {}

	virtual ~Parser() = default;

	const char *get_max_addr() const
	{ return max_addr_; }

	ResultT *parse(const char *&input) const {
		const char *start = input;
		ResultT *rv = _parse(input);
		if (input > max_addr_) max_addr_ = input;
		if (!rv) input = start;
		return rv;
	}

	bool match(const char *&input) const {
		ResultT *result = parse(input);
		if (result) {
			delete result;
			return true;
		}
		return false;
	}

protected:
	virtual ResultT *_parse(const char *&input) const = 0;

private:
	static const char *max_addr_;
};

template<typename ResultT>
const char *Parser<ResultT>::max_addr_ = nullptr;


class RegexParser : public Parser<std::string> {
private:
	regex_t *expr_;
	unsigned int data_idx_;
	string re_str;
	bool bol_only_;
public:
	RegexParser(const std::string expr, const unsigned int data_idx = 0,
			bool bol_only = true, bool match_nl = false);
	virtual ~RegexParser();
	std::string *_parse(const char *&input) const override;
};


class KeywordParser : public RegexParser {
public:
	KeywordParser(const std::string keyword);
};


class FanParser : public Parser<FanDriver> {
public:
	FanDriver *_parse(const char *&input) const override;
};


class SensorParser : public Parser<SensorDriver> {
public:
	SensorDriver *_parse(const char *&input) const override;
};


class IntListParser : public Parser<std::vector<int>> {
private:
	RegexParser int_parser_, sep_parser_;
public:
	IntListParser();
	std::vector<int> *_parse(const char *&input) const override;
};


class BracketParser : public RegexParser {
public:
	BracketParser(const std::string opening, const std::string closing, bool nl = true);
};


class TupleParser : public Parser<vector<int>> {
public:
	TupleParser() {};
	vector<int> *_parse(const char *&input) const override;
};


class SimpleLevelParser : public Parser<SimpleLevel> {
public:
	SimpleLevelParser() {}
	SimpleLevel *_parse(const char *&input) const override;
};


class ComplexLevelParser : public Parser<ComplexLevel> {
public:
	ComplexLevelParser() {}
	ComplexLevel *_parse(const char *&input) const override;
};


class ConfigParser : public Parser<Config> {
private:
	const RegexParser parser_comment;
	const FanParser parser_fan;
	const SensorParser parser_sensor;
	const SimpleLevelParser parser_simple_lvl;
	const ComplexLevelParser parser_complex_lvl;
public:
	ConfigParser();

	Config *parse_config(const char *&input)
	{ return parse(input); }

protected:
	Config *_parse(const char *&input) const override;
};


} /* namespace thinkfan */






#endif /* THINKFAN_PARSER_H_ */




