/*
 * parser.h
 *
 *  Created on: 11.01.2015
 *      Author: ich
 */

#ifndef PARSER_H_
#define PARSER_H_

#include <regex.h>
#include <vector>
#include <string>
#include <iterator>
#include <exception>
#include <string.h>

namespace thinkfan {

using namespace std;

class ParserMisdefinition : public std::exception {};

class ParserOOM : public std::exception {};

class SimpleLevel;
class Config;
class Fan;

template<typename ResultT>
class Parser {
public:
	typedef ResultT* ResultT_;

	virtual ~Parser() = default;
	virtual ResultT *parse(const std::string &input, std::string::size_type &pos) const = 0;

	bool match(const string &input, string::size_type &pos) const {
		ResultT *result = parse(input, pos);
		if (result) {
			delete result;
			return true;
		}
		return false;
	}
};


class RegexParser : public Parser<std::string> {
private:
	regex_t *expr_;
	unsigned int data_idx_;
public:
	RegexParser(const std::string expr, const unsigned int data_idx = 0);
	virtual ~RegexParser();
	std::string *parse(const std::string &input, std::string::size_type &pos) const override;
};


class KeywordParser : public RegexParser {
public:
	KeywordParser(const std::string keyword);
};


class FanParser : public Parser<Fan> {
private:
	const KeywordParser parser_fan, parser_tp_fan, parser_pwm_fan;
public:
	FanParser();
	Fan *parse(const string &input, string::size_type &pos) const override;
};

class IntListParser : public Parser<std::vector<long int>> {
private:
	RegexParser int_parser_, sep_parser_;
public:
	IntListParser();
	std::vector<long int> *parse(const std::string &input, std::string::size_type &pos) const override;
};



class BracketParser : public RegexParser {
public:
	BracketParser(const std::string opening, const std::string closing);
};

class SimpleLevelParser : public Parser<SimpleLevel> {
private:
	BracketParser round_parser_, curly_parser_, quot_parser_;
	IntListParser int_parser_;
public:
	SimpleLevelParser();
	SimpleLevel *parse(const string &input, string::size_type &pos) const override;
};



class ConfigParser : public Parser<Config> {
private:
	const RegexParser parser_comment, parser_space;
	const KeywordParser parser_sensor, parser_hwmon, parser_tp_thermal;
	const FanParser parser_fan;
	const SimpleLevelParser parser_simple_lvl;
public:
	ConfigParser();
	Config *parse(const string &input, string::size_type &pos) const override;
};
} /* namespace thinkfan */






#endif /* PARSER_H_ */




