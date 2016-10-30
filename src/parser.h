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
#include <initializer_list>

#include "drivers.h"

namespace thinkfan {

using namespace std;

class SimpleLevel;
class ComplexLevel;
class Config;
static const string tpacpi_path = "/proc/acpi/ibm";

class ErrorTracker {
protected:
	static const char *max_addr_;
};

template<typename ResultT>
class Parser : ErrorTracker {
public:
	Parser() {}

	virtual ~Parser() = default;

	const char *get_max_addr() const
	{ return max_addr_; }

	ResultT *parse(const char *&input) {
		const char *start = input;
		ResultT *rv = _parse(input);
		if (input > max_addr_) max_addr_ = input;
		if (!rv) input = start;
		return rv;
	}

	bool match(const char *&input) {
		ResultT *result = parse(input);
		if (result) {
			delete result;
			return true;
		}
		return false;
	}

protected:
	virtual ResultT *_parse(const char *&input) = 0;
};


class RegexParser : public Parser<string> {
private:
	regex_t *expr_;
	unsigned int data_idx_;
	string re_str;
public:
	RegexParser(const string expr, const unsigned int data_idx = 0, bool match_nl = false);
	virtual ~RegexParser();
	virtual string *_parse(const char *&input) override;
};


class IntParser : public Parser<int> {
public:
	virtual int *_parse(const char *&input) override;
};


class CommentParser : public Parser<string> {
private:
	RegexParser comment_parser_;
public:
	CommentParser();
	virtual string *_parse(const char *&input) override;
};


class KeywordParser : public RegexParser {
public:
	KeywordParser(const std::string keyword);
};


class FanParser : public Parser<FanDriver> {
public:
	virtual FanDriver *_parse(const char *&input) override;
};


class IntListParser : public Parser<std::vector<int>> {
private:
	IntParser int_parser_;
	RegexParser dot_parser_;
	bool allow_dot_;
public:
	IntListParser(bool allow_dot = false);
	virtual std::vector<int> *_parse(const char *&input) override;
};


class EnclosureParser : public Parser<string> {
public:
	EnclosureParser(initializer_list<string> bracket_pairs, bool nl = true);
	virtual string *_parse(const char *&input) override;

	bool open(const char *&input);
	bool close(const char *&input);
	string *content(const char *&input);
private:
	string closing_;
	string content_;
	vector<string> brackets_;
	bool nl_;
};


class BracketParser : public EnclosureParser {
public:
	BracketParser();
};


class SensorParser : public Parser<SensorDriver> {
public:
	virtual SensorDriver *_parse(const char *&input) override;
private:
	BracketParser bracket_parser_;
};


class TupleParser : public Parser<vector<int>> {
public:
	TupleParser(bool allow_dot = false) : int_list_parser_(allow_dot) {}
	virtual vector<int> *_parse(const char *&input) override;
private:
	BracketParser bracket_parser_;
	IntListParser int_list_parser_;
};


class SimpleLevelParser : public Parser<SimpleLevel> {
public:
	SimpleLevelParser() {}
	virtual SimpleLevel *_parse(const char *&input) override;
private:
	BracketParser bracket_parser_;
};


class ComplexLevelParser : public Parser<ComplexLevel> {
public:
	ComplexLevelParser() {}
	virtual ComplexLevel *_parse(const char *&input) override;
private:
	BracketParser bracket_parser_;
};


class ConfigParser : public Parser<Config> {
private:
	FanParser parser_fan;
	SensorParser parser_sensor;
	SimpleLevelParser parser_simple_lvl;
	ComplexLevelParser parser_complex_lvl;
public:
	ConfigParser();

	Config *parse_config(const char *&input)
	{ return parse(input); }

protected:
	virtual Config *_parse(const char *&input) override;
};


} /* namespace thinkfan */






#endif /* THINKFAN_PARSER_H_ */




