/**
 * @class Literal <Literal.h>
 * @author Victor Mataré
 * A Literal is a syntactic Item that represents a single String. Also known
 * as a "terminal symbol".
 */

#include "Literal.h"
#include <sstream>
#include <typeinfo>
#include <exception>


Literal::Literal(
		const Literal &init,
		const bool invert)
: Item(init, invert),
  definition(init.definition) {}


Literal::Literal(
		const grampa_string_t &init,
		const bool invert,
		const Item *producer
)
: Item(invert, producer),
  definition(init) {}


Literal* Literal::match(bool const invert, std::basic_istream<grampa_char_t> &input) const {
	grampa_char_t *in_match = new grampa_char_t[definition.length()];
	std::basic_istream<grampa_char_t>::pos_type initial_pos = input.tellg();
	Literal *rv = NULL;

	std::streamsize nchars = input.readsome(in_match, static_cast<std::streamsize>(definition.length()));

	if (nchars == static_cast<long int>(definition.length())) {
		if ((inverted != invert) != (definition == in_match)) {
			rv = new Literal(in_match);
		}
		else input.seekg(initial_pos);
	}
	return rv;
}


grampa_string_t Literal::definition_string(unsigned int const depth,
		grampa_string_t const & prefix) const {
	grampa_string_t tmp;
	if (inverted) tmp.append(GRAMPA_LITERAL(-));
	tmp.append("\"");
	tmp.append(definition);
	grampa_char_t srch[] = { GRAMPA_LITERAL(\n\r\t\f) };
	grampa_char_t repl[][3] = { GRAMPA_LITERAL(\\n), GRAMPA_LITERAL(\\r),
			GRAMPA_LITERAL(\\t), GRAMPA_LITERAL(\\f) };
	size_t i;

	for (int c = 0; c < 4; c++)
		while ((i = tmp.find(srch[c])) != tmp.npos)
			tmp.replace(i, 1, repl[c]);
	tmp.append("\"");
	return tmp;
}


bool Literal::equals(Item const *const o) const {
	Literal const *const other = dynamic_cast<Literal const *const>(o);
	if (other == NULL) return false;
	if (definition.size() != other->definition.size()) return false;
	return definition == other->definition;
}


Literal const *Literal::fixate() const { return this; }

unsigned int Literal::recursion_depth() const { return 0; }
bool Literal::is_ambiguous() const { return false; }

size_t Literal::size() const { return 1; }
size_t Literal::length() const { return definition.length(); }
Literal const &Literal::get_subject() const { return *this; }
Literal* Literal::permute(const bool invert, std::basic_istream<grampa_char_t> &input) const {
	return NULL;
}

grampa_string_t const &Literal::get_string() const {
	return definition;
}
