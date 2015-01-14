#include "parser.h"
#include "config.h"

namespace thinkfan {

using namespace std;


RegexParser::RegexParser(const string expr, const unsigned int data_idx)
: data_idx_(data_idx) {
	this->expr_ = (regex_t *) malloc(sizeof(regex_t));
	if (regcomp(this->expr_, expr.c_str(), REG_EXTENDED)) {
		throw ParserMisdefinition();
	}
}


RegexParser::~RegexParser()
{ regfree(expr_); }


std::string *RegexParser::parse(const std::string &input, std::string::size_type &pos) const {
	regmatch_t matches[data_idx_ + 1];
	std::string *rv = nullptr;

	int err;
	if (!(err = regexec(this->expr_, input.c_str() + pos, this->data_idx_ + 1, matches, 0))) {
		if (matches[data_idx_].rm_so != -1) {
			rv = new std::string(input, pos + matches->rm_so, pos + matches->rm_eo);
			pos += matches->rm_eo;
		}
	}
	else if (err == REG_ESPACE) {
		throw ParserOOM();
	}
	return rv;
}


KeywordParser::KeywordParser(const string keyword)
: RegexParser("^[[:space:]]*" + keyword + "[[:space:]]+([^[:space:]]+|\"[^\"]+\")", 1)
{}


FanParser::FanParser()
: parser_fan("fan"),
  parser_tp_fan("tp_fan"),
  parser_pwm_fan("pwm_fan")
{}

Fan *FanParser::parse(const string &input, string::size_type &pos) const
{
	Fan *fan = nullptr;
	string *path = nullptr;

	if ((path = parser_fan.parse(input, pos)))
		fan = new Fan(*path);
	else if ((path = parser_tp_fan.parse(input, pos)))
		fan = new TpFan(*path);
	else if ((path = parser_pwm_fan.parse(input, pos)))
		fan = new PwmFan(*path);

	delete path;
	return fan;
}


IntListParser::IntListParser()
: int_parser_("^[[:space:]]*([[:digit:]]+)", 1),
  sep_parser_("^([[:space:]]*,)|([[:space:]]+)", 0)
{}

vector<long int> *IntListParser::parse(const string &input, string::size_type &pos) const
{
	vector<long int> *rv = new vector<long int>();
	string *s_int;

	while ((s_int = int_parser_.parse(input, pos))) {
		const char *cs_int = s_int->c_str();
		rv->push_back(strtol(cs_int, nullptr, 0));
		delete s_int;

		string *s_sep;
		if (!(s_sep = sep_parser_.parse(input, pos))) break;
		delete s_sep;
	}

	if (rv->size() > 0) return rv;
	delete rv;
	return nullptr;
}


BracketParser::BracketParser(const string opening, const string closing)
: RegexParser("\\" + opening + "(.*)\\" + closing, 1)
{}

SimpleLevelParser::SimpleLevelParser()
: round_parser_("(", ")"),
  curly_parser_("{", "}"),
  quot_parser_("\"", "\"")
{}

SimpleLevel *SimpleLevelParser::parse(const string &input, string::size_type &pos) const
{
	SimpleLevel *rv = nullptr;
	const BracketParser *bp = &round_parser_;
	string *bracket;
	if (!((bracket = (bp = &round_parser_)->parse(input, pos))
			|| (bracket = (bp = &curly_parser_)->parse(input, pos)))) {
		return nullptr;
	}
	delete bracket;

	vector<long> *ints = nullptr;
	string *lvl_str = nullptr;
	if (!((ints = int_parser_.parse(input, pos))
			|| ((lvl_str = quot_parser_.parse(input, pos))
					&& (ints = int_parser_.parse(input, pos))))) {
		delete lvl_str;
		return nullptr;
	}

	if ((bracket = bp->parse(input, pos))) {
		delete bracket;
		if (lvl_str && ints->size() == 2) rv = new SimpleLevel(*lvl_str, (*ints)[0], (*ints)[1]);
		else if (ints->size() == 3) rv = new SimpleLevel((*ints)[0], (*ints)[1], (*ints)[2]);
	}

	return rv;
}


ConfigParser::ConfigParser()
: parser_comment("^[[:space:]]*#[^\\n\\r]*"),
  parser_space("^[[:space:]]+"),
  parser_sensor("sensor"),
  parser_hwmon("hwmon"),
  parser_tp_thermal("tp_thermal")
{}

Config *ConfigParser::parse(const string &input, string::size_type &pos) const
{
	Config *rv = nullptr;

	bool some_match;
	do {
		some_match = false;

		some_match = parser_comment.match(input, pos);
		some_match = parser_space.match(input, pos) || some_match;

		Fan *fan = parser_fan.parse(input, pos);
		some_match = fan || some_match;

		Level *level = parser_simple_lvl.parse(input, pos);
		some_match = level || some_match;
	} while(pos < input.length() - 1 && some_match);

	return rv;
}

}





