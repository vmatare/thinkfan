#ifndef GRAMPA_LITERAL_H
#define GRAMPA_LITERAL_H

#include "Item.h"
#include <exception>

class Literal : public Item {
private:
	grampa_string_t const definition;

public:
	Literal(
			const Literal &init,
			const bool invert = false
	);

	Literal(
			const grampa_string_t &init,
			const bool invert = false,
			const Item *producer = nullptr
	);


	grampa_string_t const &get_string() const;
	virtual Literal* match(bool const invert, std::basic_istream<grampa_char_t> &input) const override;
	virtual bool is_ambiguous() const override;
	virtual size_t size() const override;
	virtual size_t length() const override;
	virtual grampa_string_t definition_string(
			unsigned int const depth,
			grampa_string_t const& prefix) const override;
	virtual unsigned int recursion_depth() const override;
	virtual Literal const &get_subject() const override;
	virtual Literal* permute(const bool invert, std::basic_istream<grampa_char_t> &input) const override;
	virtual bool equals(Item const* const o) const override;
	virtual Literal const *fixate() const override;
};

/*
Literal _L(char const *c) {
	grampa_string_t def(c);
	return Literal(def);
}*/

#endif
