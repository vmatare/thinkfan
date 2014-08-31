#ifndef GRAMPA_ALTERNATION_H
#define GRAMPA_ALTERNATION_H

#include "Item.h"
#include "Literal.h"
#include "Constituent.h"

class Alternation : public Constituent {
public:
	Alternation(
			const bool invert = false,
			const Item *const producer = nullptr
	);

	Alternation(
			const Item &init,
			const bool invert = false
	);

	explicit Alternation(
			std::initializer_list<const char *> literals
	);
	explicit Alternation(
			std::initializer_list<const Item> literals
	);


	virtual ~Alternation();

	virtual Alternation* permute(bool const invert, std::basic_istream<grampa_char_t> &input) const override;
	virtual Alternation* match(bool const invert, std::basic_istream<grampa_char_t> &input) const override;
	virtual grampa_string_t definition_string(
			unsigned int const depth, grampa_string_t const &prefix) const override;
	virtual bool is_ambiguous() const override;
	virtual size_t length() const override;
	virtual Item const *fixate() const override;
};



#endif
