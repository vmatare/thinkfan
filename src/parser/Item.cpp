#include "Item.h"
#include "Concatenation.h"
#include "Alternation.h"

unsigned long int Item::item_count = 0;
unsigned long int Item::match_count = 0;
std::basic_istream<grampa_char_t>::pos_type Item::errorOffset = 0;


Item::Item(
		const bool invert,
		const Item *const producer
)
: inverted(invert),
  producer(producer),
  id(item_count)
{
	item_count++;
	string_rendered = new bool();
}


Item::Item(
		const Item &init,
		const bool invert
)
: Item(invert != init.inverted, init.producer) {}


Item::~Item() {}


bool Item::operator==(Item const& other) const {
	return this->equals(&other);
}
bool Item::operator!=(Item const& other) const {
	return !this->equals(&other);
}

bool Item::is_inverted() const { return inverted; }
Item const* Item::get_producer() const { return producer; }
unsigned int Item::recursion_depth() const { return 0; }
Item const& Item::get_subject() const { return *this; }

Concatenation Item::operator + (Item const &other) {
	Concatenation rv = new Concatenation();
	rv.add(*this);
	rv.add(other);
	return rv;
}

Concatenation Item::operator + (Concatenation const &concat) {
	Concatenation rv = new Concatenation();
	rv.add(*this);
	rv.merge(concat);
	return rv;
}

Alternation Item::operator | (Item const &other) {
	Alternation rv = new Alternation();
	rv.add(*this);
	rv.add(other);
	return rv;
}

Alternation Item::operator | (Alternation const &alternation) {
	Alternation rv = new Alternation();
	rv.add(*this);
	return rv;
}

Alternation Item::operator * () {
	Alternation rv;
	rv.add(*this + rv);
	rv.add(Literal(""));
	return rv;
}

Concatenation Item::operator + () {
	Concatenation rv;
	rv.add(*this);
	rv.add(Literal("") | rv);
	return rv;
}
