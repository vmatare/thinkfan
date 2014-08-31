#ifndef GRAMPA_ITEM_H
#define GRAMPA_ITEM_H

#define GRAMPA_LITERAL(x) #x

#include <string>
#include <fstream>

typedef char grampa_char_t;
typedef std::basic_string<grampa_char_t> grampa_string_t;

// Needed for operators
class Concatenation;
class Alternation;


class Item {
protected:
	bool const inverted;
	Item const *const producer;
	unsigned long int const id;
	bool *string_rendered;
	static unsigned long int item_count;
	static unsigned long int match_count;

public:
	static std::basic_istream<grampa_char_t>::pos_type errorOffset;

	explicit Item(
			const bool invert=false,
			const Item *const producer=nullptr);

	explicit Item(
			const Item &init,
			const bool invert=false);

	virtual ~Item();

	virtual bool is_inverted() const;
	virtual Item const* get_producer() const;
	virtual Item const& get_subject() const;

	virtual Item* match(bool const invert, std::basic_istream<grampa_char_t> &input) const = 0;

	virtual Item* permute(bool const invert, std::basic_istream<grampa_char_t> &input) const = 0;

	virtual grampa_string_t definition_string(
			const unsigned int depth, grampa_string_t const &prefix) const = 0;
	virtual bool is_ambiguous() const = 0;
	virtual size_t length() const = 0;
	virtual unsigned int recursion_depth() const = 0;
	virtual size_t size() const = 0;
	virtual bool equals(Item const *const o) const = 0;
	virtual Item const *fixate() const = 0;

	bool operator == (Item const &other) const;
	bool operator != (Item const &other) const;
	Concatenation operator + (Item const &other);
	Concatenation operator + (Concatenation const &concat);
	Alternation operator | (Item const &other);
	Alternation operator | (Alternation const &alternation);
	Alternation operator * ();
	Concatenation operator + ();

};

template<class ItemT> ItemT operator ! (ItemT i) {
	ItemT rv(i, true);
	return rv;
}


#endif
