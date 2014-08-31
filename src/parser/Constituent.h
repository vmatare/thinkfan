#ifndef GRAMPA_CONSTITUENT_H
#define GRAMPA_CONSTITUENT_H

#include "Item.h"
#include <deque>
#include <type_traits>


typedef std::deque<Item const *> grampa_deque_t;

class Constituent : public Item {
private:
	grampa_deque_t _definition;
protected:
	unsigned int *m_recursionDepth;
	long int _length;

public:
	explicit Constituent(
			const bool invert = false,
			const Item *const producer = nullptr
	);
	explicit Constituent(
			const Item &init,
			const bool invert=false
	);

	explicit Constituent(
			const Constituent &init
	);
	explicit Constituent(
			const Constituent &init,
			Item &replacement,
			grampa_deque_t::iterator pos
	);


	explicit Constituent(
			std::initializer_list<const char *> literals
	);
	explicit Constituent(
			std::initializer_list<const Item> literals
	);

	grampa_deque_t const & definition() const;

	virtual ~Constituent();

	virtual unsigned int recursion_depth() const override;
	virtual void add(Item const &);
	virtual void add(char *);
	virtual size_t size() const;
	virtual bool equals(Item const* const) const override;
	virtual grampa_deque_t::const_iterator begin() const;
	virtual grampa_deque_t::const_iterator end() const;
	virtual void merge(Item const &);
};


#endif
