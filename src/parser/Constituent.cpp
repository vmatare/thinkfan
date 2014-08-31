#include "Constituent.h"
#include "Literal.h"
#include <typeinfo>

Constituent::Constituent(
		bool invert,
		Item const *const producer
)
: Item(invert, producer),
  _length(0)
{
	m_recursionDepth = new unsigned int;
	*m_recursionDepth = 0;
}


Constituent::Constituent(
		const Item &init,
		const bool invert
)
: Item(init, invert),
  _length(0)
{
	m_recursionDepth = new unsigned int;
	*m_recursionDepth = 0;
}


Constituent::Constituent(
		const Constituent &init
)
: Constituent(init.is_inverted(), init.get_producer()) {
	_length = init._length;
	_definition = init.definition();
}


Constituent::Constituent(
		const Constituent &init,
		Item &replacement,
		grampa_deque_t::iterator pos
)
: Constituent(init) {
	_definition.erase(pos);
	_definition.insert(pos, &replacement);
}


Constituent::Constituent(
		std::initializer_list<const char *> literals
)
: Item(false),
  _length(0)
{
	for (const char *c : literals) {
		add(Literal(grampa_string_t(c)));
	}
}


Constituent::Constituent(
		std::initializer_list<const Item> items
)
: Item(false),
  _length(0)
{
	for (const Item &i : items) {
		add(i);
	}
}


Constituent::~Constituent() {
	delete m_recursionDepth;
}

unsigned int Constituent::recursion_depth() const {
	return *m_recursionDepth;
}

grampa_deque_t const & Constituent::definition() const { return _definition; }

bool Constituent::equals(Item const* const o) const {
	if (o == NULL) return false;
	Constituent const* const other = dynamic_cast<Constituent const* const>(o);
	if (other == NULL) return false;
	if (size() != other->size()) return false;
	grampa_deque_t::const_iterator itA = begin();
	for (grampa_deque_t::const_iterator itB = other->begin();
			itA != end() && itB != other->end();
			itA++, itB++) {
		if (**itA != **itB) return false;
	}
	return true;
}

grampa_deque_t::const_iterator Constituent::begin() const {
	return definition().begin();
}
grampa_deque_t::const_iterator Constituent::end() const {
	return definition().end();
}

void Constituent::add(Item const &i) {
	_definition.push_back(&i);
	_length += i.length();
}

void Constituent::add(char *s) {
	Literal l((grampa_string_t(s)));
	add(l);
}

size_t Constituent::size() const { return definition().size(); }

void Constituent::merge(Item const &i) {
	try {
		Constituent const &a = dynamic_cast<Constituent const &>(i);
		for (Item const *i : a.definition()) add(*i);
	}
	catch (std::bad_cast &e) {
		add(i);
	}
}

