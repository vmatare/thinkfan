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
#include <tuple>
#include <string.h>

namespace thinkfan {

using namespace std;

class ParserMisdefinition : public std::exception {};

class ParserOOM : public std::exception {};


template<typename ResultT>
class Parser {
public:
	typedef ResultT ResultT_;

	virtual ~Parser() = default;
	virtual ResultT *parse(const std::string &input, std::string::size_type &pos) const = 0;
};



template<typename ResultT, class... ParserTs>
class ConcatParser;


template<typename ResultT, typename P1>
class ConcatParser<ResultT, P1> : public Parser<ResultT> {
private:
	P1 piece1_;
	tuple<ResultT *> cat_parse(const string &input, string::size_type &pos, P1 piece1) const {
		return tie(piece1.parse(input, pos));
	}
public:
	ConcatParser(P1 p1)
	: piece1_(p1)
	{}

	ResultT *parse(const string &input, string::size_type &pos) const override {
		return piece1_.parse(input, pos);
	}
};




template<typename ResultT, class P1, class... ParserTs>
class ConcatParser<ResultT, P1, ParserTs...> : public Parser<ResultT> {
private:
	P1 piece1_;
	tuple<ParserTs...> nextPieces_;
	/*tuple<ResultT *> cat_parse(const string &input, string::size_type &pos, P1 piece1) const {
		return tie(piece1.parse(input, pos));
	}*/
	tuple<typename P1::ResultT_, typename ParserTs::ResultT_...> cat_parse(const string &input, string::size_type &pos, P1 piece1, tuple<ParserTs...> nextPieces) const {
		return tuple_cat(tie(piece1.parse(input, pos)), nextPieces.parse(input, pos));
	}
public:
	ConcatParser(P1 piece1, ParserTs... nextPieces)
	: piece1_(piece1),
	  nextPieces_(nextPieces...)
	{}

	ResultT *parse(const string &input, string::size_type &pos) const override {
		return new ResultT(cat_parse(input, pos, piece1_, nextPieces_));
	}
};



class RegexParser : public Parser<std::string> {
private:
	regex_t *expr_;
	const unsigned int data_idx_;
public:
	RegexParser(const std::string expr, const unsigned int data_idx);
	virtual ~RegexParser();
	std::string *parse(const std::string &input, std::string::size_type &pos) const override;
};


class KeywordParser : public RegexParser {
public:
	KeywordParser(const std::string keyword);
};


class IntListParser : public Parser<std::vector<long int>> {
private:
	RegexParser int_parser_, sep_parser_;
public:
	IntListParser();
	std::vector<long int> *parse(const std::string &input, std::string::size_type &pos) const override;
};


class Level;

/*class BracketParser : public RegexParser {
public:
	BracketParser(const std::string opening, const std::string closing);
};*/

class SimpleLimitParser : public ConcatParser<Level, RegexParser, IntListParser, RegexParser> {
private:
	/*BracketParser round_parser_;
	BracketParser curly_parser_;
	BracketParser quot_parser_;*/
public:
	SimpleLimitParser();
	Level *parse(const string &input, string::size_type &pos) const override;
};

} /* namespace thinkfan */






#endif /* PARSER_H_ */




