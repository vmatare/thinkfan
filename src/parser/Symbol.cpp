/*
 * Symbol.cpp
 *
 *  Created on: 19.01.2014
 *      Author: ich
 */

#include "Symbol.h"

template<typename ApplicationT, class GrammarT>
Symbol<ApplicationT, GrammarT>::Symbol
(
		const Compiler<ApplicationT, GrammarT> &compiler,
		GrammarT subject
)
: AbstractDecorator<GrammarT>(subject),
  _compiler(compiler)
  {}


template<typename ApplicationT, class GrammarT>
Symbol<ApplicationT, GrammarT>::Symbol
(
		Symbol<ApplicationT, GrammarT> &s
)
: AbstractDecorator<GrammarT>(s.get_subject()),
  _compiler(s._compiler)
  {}


template<typename ApplicationT, class GrammarT>
Symbol<ApplicationT, GrammarT>* Symbol<ApplicationT, GrammarT>::permute
(
		const bool invert,
		std::basic_istream<grampa_char_t> &input
) const
{
	GrammarT *rv = AbstractDecorator<GrammarT>::_subject.permute(invert, input);
	return rv ? new Symbol<ApplicationT, GrammarT>(_compiler, *rv) : NULL;
}


template<typename ApplicationT, class GrammarT>
Symbol<ApplicationT, GrammarT> const *Symbol<ApplicationT, GrammarT>::fixate() const
{
	Item *tmp =  AbstractDecorator<GrammarT>::_subject.fixate();

	Symbol<ApplicationT, GrammarT> *rv = new Symbol<ApplicationT, GrammarT>(
			_compiler,
			AbstractDecorator<GrammarT>::_subject.fixate()
	);
	return rv;
}


template<typename ApplicationT, class GrammarT>
grampa_string_t Symbol<ApplicationT, GrammarT>::definition_string
(
		const unsigned int depth,
		const grampa_string_t &prefix
) const
{
	return AbstractDecorator<GrammarT>::_name
			+ " = "
			+ AbstractDecorator<GrammarT>::_subject.definition_string(depth + 1, prefix);
}


template<typename ApplicationT, class GrammarT>
Symbol<ApplicationT, GrammarT>* Symbol<ApplicationT, GrammarT>::match
(
		bool invert,
		std::basic_istream<grampa_char_t> &input
) const
{
	auto *rv = AbstractDecorator<GrammarT>::_subject.match(invert, input);
	if (rv) {
		Symbol<ApplicationT, GrammarT> *result = new Symbol<ApplicationT, GrammarT>(_compiler, *rv);
		delete rv;
		return result;
	}
	else {
		return nullptr;
	}
}


template<typename ApplicationT, class GrammarT>
const Compiler<ApplicationT, GrammarT> &Symbol<ApplicationT, GrammarT>::get_compiler() const
{
	return _compiler;
}



