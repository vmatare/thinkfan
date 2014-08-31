#ifndef SYMBOL_H_
#define SYMBOL_H_

#include "AbstractDecorator.h"
#include "Item.h"

template<typename ApplicationT, typename GrammarT>
class Symbol;

template<typename _App_T, class _Grammar_T>
class Compiler {
public:
	virtual ~Compiler() = 0;

	virtual _App_T compile(Symbol<_App_T, _Grammar_T> s) = 0;
};


template<typename ApplicationT, typename GrammarT>
class Symbol : public AbstractDecorator<GrammarT> {
private:
	Compiler<ApplicationT, GrammarT> const &_compiler;

public:
	Symbol(Compiler<ApplicationT, GrammarT> const &compiler, GrammarT subject);

	Symbol(Symbol<ApplicationT, GrammarT> &s);

	virtual ~Symbol();

	virtual Symbol<ApplicationT, GrammarT> *match
	(
			bool const invert,
			std::basic_istream<grampa_char_t> &input
	) const override;

	virtual Symbol<ApplicationT, GrammarT> *permute
	(
			bool const invert,
			std::basic_istream<grampa_char_t> &input
	) const override;

	virtual grampa_string_t definition_string
	(
			const unsigned int depth,
			grampa_string_t const &prefix
	) const override;

	virtual const Compiler<ApplicationT, GrammarT> &get_compiler() const;

	virtual Symbol<ApplicationT, GrammarT> const *fixate() const override;
};


template <typename AppT, typename GrammarT>
static Symbol<AppT, GrammarT> get_Symbol(
		const Compiler<AppT, GrammarT> &compiler,
		GrammarT subject)
{
	Symbol<AppT, GrammarT> rv(compiler, subject);
	return rv;
}

#endif /* SYMBOL_H_ */
