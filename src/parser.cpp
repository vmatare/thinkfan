/********************************************************************
 * parser.cpp: Recursive descent parser for the config.
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
#include "parser.h"
#include "config.h"
#include <memory>

namespace thinkfan {

using namespace std;


const char *ErrorTracker::max_addr_ = nullptr;

static RegexParser separator_parser("^([[:space:]]*,)|^([[:space:]]+)", 0);
static CommentParser comment_parser;
static RegexParser space_parser("^[[:space:]]+", 0, true);
static RegexParser blank_parser("^[[:blank:]]+", 0, false);


RegexParser::RegexParser(const string expr, const unsigned int data_idx, bool match_nl)
: data_idx_(data_idx),
  re_str(expr)
{
	this->expr_ = new regex_t;
	if (regcomp(this->expr_, re_str.c_str(), REG_EXTENDED | (match_nl ? 0 : REG_NEWLINE))) {
		throw Bug("RegexParser: Invalid regular expression");
	}
}


RegexParser::~RegexParser()
{
	regfree(expr_);
	delete expr_;
}


string *RegexParser::_parse(const char *&input)
{
	regmatch_t matches[data_idx_ + 1];
	string *rv = nullptr;

	int err;
	if (!(err = regexec(this->expr_, input, this->data_idx_ + 1, matches, 0))) {
		int so = matches[data_idx_].rm_so;
		int eo = matches[data_idx_].rm_eo;
		if (so != -1 && matches[0].rm_so == 0) {
			rv = new string(input, so, eo - so);
			input += matches->rm_eo;
		}
	}
	else if (err == REG_ESPACE) {
		delete rv;
		throw ParserOOM();
	}
	return rv;
}


int *IntParser::_parse(const char *&input)
{
	char *end;
	long l = strtol(input, &end, 0);
	if (end == input || l < numeric_limits<int>::min() || l > numeric_limits<int>::max())
		return nullptr;

	input = end;
	return new int(static_cast<int>(l));
}


CommentParser::CommentParser()
: comment_parser_("^[[:space:]]*#(.*)$", 1)
{}


string *CommentParser::_parse(const char *&input)
{
	space_parser.match(input);
	string *rv = nullptr;
	string *tmp;
	while ((tmp = comment_parser_.parse(input))) {
		if (!rv)
			rv = tmp;
		else {
			*rv += *tmp;
			delete tmp;
		}
		space_parser.match(input);
	}
	return rv;
}


KeywordParser::KeywordParser(const string keyword)
: RegexParser("^[[:space:]]*" + keyword + "[[:space:]]+([^[:space:]]+|\"[^\"]+\")", 1)
{}


FanDriver *FanParser::_parse(const char *&input)
{
	FanDriver *fan = nullptr;
	unique_ptr<string> path;

	if ((path = unique_ptr<string>(KeywordParser("fan").parse(input))))
		throw ConfigError(MSG_CONF_FAN_DEPRECATED);
	else if ((path = unique_ptr<string>(KeywordParser("tp_fan").parse(input))))
		fan = new TpFanDriver(*path);
	else if ((path = unique_ptr<string>(KeywordParser("pwm_fan").parse(input))))
		fan = new HwmonFanDriver(*path);

	return fan;
}


SensorDriver *SensorParser::_parse(const char *&input)
{
	SensorDriver *sensor = nullptr;
	unique_ptr<string> path;

	if ((path = unique_ptr<string>(KeywordParser("sensor").parse(input))))
		throw ConfigError(MSG_CONF_SENSOR_DEPRECATED);
	else if ((path = unique_ptr<string>(KeywordParser("tp_thermal").parse(input))))
		sensor = new TpSensorDriver(*path, false);
	else if ((path = unique_ptr<string>(KeywordParser("hwmon").parse(input))))
		sensor = new HwmonSensorDriver(*path, false);
	else if ((path = unique_ptr<string>(KeywordParser("atasmart").parse(input)))) {
#ifdef USE_ATASMART
		sensor = new AtasmartSensorDriver(*path);
#else
		error<SystemError>(MSG_CONF_ATASMART_UNSUPP);
#endif /* USE_ATASMART */
	}
	else if ((path = unique_ptr<string>(KeywordParser("nv_thermal").parse(input)))) {
#ifdef USE_NVML
		sensor = new NvmlSensorDriver(*path, false);
#else
		error<SystemError>(MSG_CONF_NVML_UNSUPP);
#endif /* USE_NVML */
	}

	if (sensor) {
		blank_parser.match(input);
		unique_ptr<vector<int>> correction(TupleParser().parse(input));
		if (correction)
			sensor->set_correction(*correction);
	}

	return sensor;
}


IntListParser::IntListParser(bool allow_dot)
: dot_parser_("^\\."),
  allow_dot_(allow_dot)
{}


vector<int> *IntListParser::_parse(const char *&input)
{
	unique_ptr<vector<int>> rv(new vector<int>());
	unique_ptr<string> dot;
	unique_ptr<int> i;

	do {
		dot.reset(nullptr);

		if ( (void)i.reset(int_parser_.parse(input)), i)
			rv->push_back(*i);
		else if (allow_dot_ && ( (void)dot.reset(dot_parser_.parse(input)), dot))
			rv->push_back(numeric_limits<int>::max());
		else
			return nullptr;
		if (!(separator_parser.match(input) || comment_parser.match(input)))
			break;

		comment_parser.match(input);
	} while(i || dot);

	if (rv->size() > 0)
		return rv.release();
	return nullptr;
}


EnclosureParser::EnclosureParser(initializer_list<string> bracket_pairs, bool nl)
: brackets_(bracket_pairs),
  nl_(nl)
{
	if (bracket_pairs.size() < 2 || bracket_pairs.size() % 2 != 0)
		throw Bug("EnclosureParser: Unbalanced bracket definition");
}


string *EnclosureParser::_parse(const char *&input)
{
	blank_parser.match(input);
	vector<string>::const_iterator it = brackets_.begin();
	unique_ptr<string> rv;
	do {
		string re_pfx("^");
		if (*it == "(" || *it == "{")
			re_pfx += "\\";
		RegexParser re_o(re_pfx + *it++);
		rv.reset(re_o.parse(input));
		if (rv) {
			closing_ = re_pfx + *it;
			content_ = "^[^" + *it + "]*";
		}
		++it;
	} while (!rv && it != brackets_.end());

	return rv.release();
}


bool EnclosureParser::open(const char *&input)
{ return match(input); }


bool EnclosureParser::close(const char *&input)
{ return RegexParser(closing_).match(input); }


string *EnclosureParser::content(const char *&input)
{ return RegexParser(content_, 0, nl_).parse(input); }


BracketParser::BracketParser()
: EnclosureParser({"(", ")", "{", "}"})
{}


vector<int> *TupleParser::_parse(const char *&input)
{
	if (!bracket_parser_.open(input))
		return nullptr;

	unique_ptr<vector<int>> rv(int_list_parser_.parse(input));
	if (bracket_parser_.close(input))
		return rv.release();
	else
		return nullptr;
}


SimpleLevel *SimpleLevelParser::_parse(const char *&input)
{
	if (!bracket_parser_.open(input))
		return nullptr;

	unique_ptr<vector<int>> ints;

	comment_parser.match(input);

	EnclosureParser quot({"\"", "\""});
	if (quot.open(input)) {
		unique_ptr<string> lvl_str(quot.content(input));
		if (quot.close(input) && lvl_str &&
				(comment_parser.match(input)
						|| separator_parser.match(input))
		) {
			comment_parser.match(input);
			ints.reset(IntListParser().parse(input));
			if (ints && ints->size() == 2 && bracket_parser_.close(input))
				return new SimpleLevel(*lvl_str, (*ints)[0], (*ints)[1]);
		}
	}
	else {
		ints.reset(IntListParser().parse(input));
		if (ints && ints->size() == 3 && bracket_parser_.close(input))
			return new SimpleLevel((*ints)[0], (*ints)[1], (*ints)[2]);
	}

	return nullptr;
}


ComplexLevel *ComplexLevelParser::_parse(const char *&input)
{
	unique_ptr<string> lvl_str;
	unique_ptr<vector<int>> lower_lim, upper_lim;
	unique_ptr<int> lvl_int;

	if (!bracket_parser_.open(input))
		return nullptr;

	comment_parser.match(input);
	space_parser.match(input);

	EnclosureParser quot({"\"", "\""});
	if (quot.open(input)) {
		lvl_str.reset(quot.content(input));
		if (!quot.close(input))
			return nullptr;
	}
	else if (!(lvl_int = unique_ptr<int>(IntParser().parse(input))))
		return nullptr;

	TupleParser tp(true);
	RegexParser sep_parser("^[[:space:]]+|^[[:space:]]*,?[[:space:]]*", 0, true);

	comment_parser.match(input);
	sep_parser.match(input);
	comment_parser.match(input);

	lower_lim = unique_ptr<vector<int>>(tp.parse(input));
	comment_parser.match(input);
	sep_parser.match(input);
	comment_parser.match(input);

	upper_lim = unique_ptr<vector<int>>(tp.parse(input));
	space_parser.match(input);
	comment_parser.match(input);

	if (!(lower_lim && upper_lim))
		return nullptr;

	if (!bracket_parser_.close(input))
		return nullptr;

	if (lvl_str)
		return new ComplexLevel(*lvl_str, *lower_lim, *upper_lim);
	else if (lvl_int)
		return new ComplexLevel(*lvl_int, *lower_lim, *upper_lim);
	else
		return nullptr;
}


ConfigParser::ConfigParser()
: parser_fan(),
  parser_sensor()
{}


Config *ConfigParser::_parse(const char *&input)
{
	// Use smart pointers here since we may cause an exception (rv->add_*()...)
	unique_ptr<Config> rv(new Config());

	bool some_match;
	do {
		some_match = comment_parser.match(input)
				|| space_parser.match(input)
				|| rv->add_fan(unique_ptr<FanDriver>(parser_fan.parse(input)))
				|| rv->add_sensor(unique_ptr<SensorDriver>(parser_sensor.parse(input)))
				|| rv->add_level(unique_ptr<SimpleLevel>(parser_simple_lvl.parse(input)))
				|| rv->add_level(unique_ptr<ComplexLevel>(parser_complex_lvl.parse(input)));
	} while(*input != 0 && some_match);

	if (*input != 0 && !some_match) return nullptr;

	return rv.release();
}

}





