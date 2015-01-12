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

/*
BracketParser::BracketParser(const string opening, const string closing)
: RegexParser("\\" + opening + "(.*)\\" + closing, 1)
{}*/

SimpleLimitParser::SimpleLimitParser()
: ConcatParser(RegexParser("\\(", 0), IntListParser(), RegexParser("\\)", 0))
{}

Level *SimpleLimitParser::parse(const string &input, string::size_type &pos) const
{
}

}
