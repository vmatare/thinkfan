#include "Alternation.h"
#include "Literal.h"
#include <sstream>
#include <typeinfo>
#include <assert.h>
#include <stdarg.h>

Alternation::Alternation(
		const bool invert,
		const Item *const producer
)
: Constituent(invert, producer)
{}


Alternation::Alternation(
		const Item &init,
		const bool invert
)
: Constituent(init, invert) {}


Alternation::Alternation(
		std::initializer_list<char const *> literals)
: Constituent(literals) {}


Alternation::Alternation(
		std::initializer_list<Item const> literals)
: Constituent(literals) {}


Alternation::~Alternation() {}


Alternation* Alternation::match(bool const invert, std::basic_istream<grampa_char_t> &input) const
{
	if (definition().size() <= 0) return nullptr;
	match_count++;
	Item* tmp;
	Alternation* rv = new Alternation(false, this);
	std::basic_istream<grampa_char_t>::pos_type initial_pos = input.tellg();

	for (grampa_deque_t::const_iterator it = definition().begin(); it != definition().end(); it++) {
		tmp = (*it)->match(invert != inverted, input);
		if (tmp != nullptr)
			rv->add(*tmp);
		else if (inverted != invert) return nullptr;
		input.seekg(initial_pos);
	}
	if (rv->size() == 0) {
		delete rv;
		return nullptr;
	}
	else {
		input.seekg(initial_pos + static_cast<std::basic_istream<grampa_char_t>::pos_type>(rv->length()));
		return rv;
	}
}


Alternation* Alternation::permute(bool const invert, std::basic_istream<grampa_char_t> &input) const
{
	Item *permuted_element = nullptr;
	Alternation *rv = new Alternation(this->inverted);
	for (grampa_deque_t::const_iterator it = definition().begin();
			it != definition().end(); it++) {
		if (!permuted_element) {
			permuted_element = (*it)->permute(invert, input);
			if (permuted_element)
				rv->add(*permuted_element);
			else rv->add(**it);
		}
		else rv->add(**it);

	}
	return nullptr;
}

bool Alternation::is_ambiguous() const
{ return definition().size() > 1; }


size_t Alternation::length() const
{ return definition().front()->length(); }


grampa_string_t Alternation::definition_string(
		const unsigned int depth,
		const grampa_string_t& prefix
) const
{
	if (depth < *m_recursionDepth) {
		std::stringstream rv;
		rv << "[alternation_" << id << "]";
		return rv.str();
	}
	grampa_string_t tmp;
	if (inverted) tmp += GRAMPA_LITERAL(-);
	if (Constituent::definition().size() > 1) tmp += "(";
	(*m_recursionDepth)++;
	for (Item const *i : definition()) {
		grampa_string_t pref(prefix + "\t");
		tmp.append(i->definition_string(depth, prefix));
		tmp.append(" | ");
	}
	(*m_recursionDepth)--;
	tmp = tmp.substr(0, tmp.length() - 3);
	if (Constituent::definition().size() > 1) tmp += ")";
	tmp += "\n";
	return tmp;
}


Item const *Alternation::fixate() const
{ return definition().front()->fixate(); }


