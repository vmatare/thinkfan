#ifndef GRAMPA_CONCATENATION_H_
#define GRAMPA_CONCATENATION_H_

#include "Item.h"
#include "Literal.h"
#include "Constituent.h"

class Concatenation : public Constituent {
public:
	Concatenation(
			bool invert = false,
			Item const *const producer = NULL
	);

	Concatenation(
			const Concatenation &init,
			const bool invert = false
	);

	explicit Concatenation(
			std::initializer_list<const char *> literals
	);
	explicit Concatenation(
			std::initializer_list<const Item> literals
	);



	virtual ~Concatenation();

	virtual Concatenation* permute(bool const invert, std::basic_istream<grampa_char_t> &input) const override;
	virtual Concatenation* match(bool const invert, std::basic_istream<grampa_char_t> &input) const override;
	virtual grampa_string_t definition_string(
			unsigned int const depth, grampa_string_t const& prefix) const override;
	virtual bool is_ambiguous() const override;
	virtual size_t length() const override;
	virtual Concatenation const *fixate() const override;
};


#endif /* CONCATENATION_H_ */
