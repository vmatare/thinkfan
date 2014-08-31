/**
 * A Concatenation represents an ordered sequence of Constituents.
 * It only matches if all members are matched in order.
 * @author Victor Mataré
 */

#include "Concatenation.h"
#include <sstream>
#include <typeinfo>

Concatenation::Concatenation(bool invert,
		Item const *const producer
)
: Constituent(invert, producer) {}

Concatenation::Concatenation(
		const Concatenation& init,
		const bool invert
)
: Constituent(init, invert) {}


Concatenation::Concatenation(
		std::initializer_list<char const *> literals)
: Constituent(literals) {}


Concatenation::Concatenation(
		std::initializer_list<Item const> literals)
: Constituent(literals) {}



Concatenation::~Concatenation() {}

Concatenation* Concatenation::match(bool invert, std::basic_istream<grampa_char_t> &input) const {
	match_count++;
	if (definition().size() == 0) return nullptr;
	Concatenation *rv = new Concatenation(false, this);
	Concatenation *tmp;
	Item *res = nullptr;

	std::basic_istream<grampa_char_t>::pos_type initial_pos = input.tellg();

	for (grampa_deque_t::const_iterator it = definition().begin(); it != definition().end(); it++) {
		res = (*it)->match(invert != inverted, input);
		if (res != nullptr) {
			rv->add(*res);
		}
		else {
			if (rv->size() > 0) {
				input.seekg(initial_pos);
				if ((tmp = rv->permute(inverted != invert, input)) != nullptr
						&& tmp != rv) {
					rv = tmp;
					it = definition().begin() + rv->size() - 1;
				}
				else {
					if (errorOffset < input.tellg())
						errorOffset = input.tellg();
					return nullptr;
				}
			}
			else return nullptr;
		}
	}

	if (rv->size() == 0) {
		delete rv;
		return nullptr;
	}
	else return rv;
}

/**
 * Finds this Concatenation's next permutation. The Concatenation itself is
 * ordered and thus only permutable if at least one of its elements is
 * permutable.
 * @return A new Concatenation if it was permuted, null if there are no
 * new permutations.
 */
Concatenation *Concatenation::permute(bool const invert, std::basic_istream<grampa_char_t> &input) const {
	Concatenation *rv;
	Item *tmp = nullptr, *p = nullptr;
	grampa_deque_t rematch = grampa_deque_t();

	std::basic_iostream<grampa_char_t>::pos_type initial_pos = input.tellg();

	do {
		grampa_deque_t::const_reverse_iterator it = definition().rbegin();

		rv = new Concatenation(inverted, get_producer());

		// try to permute starting from the end...
		std::basic_iostream<grampa_char_t>::pos_type removed_len =
				static_cast<std::basic_iostream<grampa_char_t>::pos_type>(definition().back()->length());
		input.seekg(initial_pos
				+ static_cast<std::basic_iostream<grampa_char_t>::pos_type>(length())
				- removed_len);
		while (it != definition().rend() && (p = (*it)->permute(invert, input)) == nullptr) {
			rematch.push_front(*it);
			removed_len += (*(++it))->length();
			input.seekg(initial_pos + static_cast<std::basic_iostream<grampa_char_t>::pos_type>(length()) - removed_len);
		}
		if (p == nullptr) {
			input.seekg(initial_pos
					+ static_cast<std::basic_iostream<grampa_char_t>::pos_type>(length())
					- removed_len);
			return nullptr;
		}

		// ...ok, found a permutation p, so keep everything up to p.
		for (size_t i = 0; i < definition().size() - rematch.size() - 1; i++)
			rv->add(*definition().at(i));
		rv->add(*p);

		// now rematch everything after p.
		for (grampa_deque_t::iterator ri = rematch.begin();
				ri != rematch.end(); ri++) {
			input.seekg(initial_pos
					+ static_cast<std::basic_iostream<grampa_char_t>::pos_type>(rv->length()));
			tmp = (*ri)->get_producer()->match(invert, input);

			// rematching failed, so this permutation is skipped.
			if (tmp == nullptr) {
				delete p;
				break;
			}
			rv->add(*tmp);
		}
	} while (rematch.size() < definition().size() && tmp == nullptr);

	return rv;
}

grampa_string_t Concatenation::definition_string(
		const unsigned int depth, const grampa_string_t& prefix) const {
	if (depth < *m_recursionDepth) {
		std::stringstream s;
		s << "[concatenation_" << id << "]";
		return s.str();
	}
	if (definition().size() == 0) return "Ø";
	grampa_string_t rv;
	if (definition().size() > 1) rv += "\n" + prefix;
	if (inverted) rv += GRAMPA_LITERAL(-);
	if (definition().size() > 1) rv += "(";
	(*m_recursionDepth)++;
	for (const Item *i : definition()) {
		rv += i->definition_string(depth-1, prefix + "\t");
		rv += ", ";
	}
	(*m_recursionDepth)--;
	if (definition().size() > 0) rv = rv.substr(0, rv.length() - 2);
	if (definition().size() > 1) rv += ")";
	return rv;
}


bool Concatenation::is_ambiguous() const {
	for (const Item *i : definition()) {
		if (i->is_ambiguous()) return true;
	}
	return false;
}
size_t Concatenation::length() const {
	return _length;
}

Concatenation const *Concatenation::fixate() const {
	Concatenation *rv = new Concatenation();

	for (const Item *i : definition()) {
		rv->add(i->is_ambiguous() ? *(i->fixate()) : *i);
	}

	return rv;
}

