/*
 * AbstractDecorator.h
 *
 *  Created on: 12.12.2011
 *      Author: ich
 */

#ifndef ABSTRACTDECORATOR_H_
#define ABSTRACTDECORATOR_H_

#include "Constituent.h"

template<class SubjectT>
class AbstractDecorator : public SubjectT {
protected:
	SubjectT _subject;

	AbstractDecorator(SubjectT subject)
	: _subject(subject)
	{}

public:
	virtual ~AbstractDecorator();

	virtual bool is_inverted() const override
	{ return _subject.is_inverted(); }

	virtual Item const* get_producer() const override
	{ return _subject.get_producer(); }

	virtual SubjectT *match(bool const invert, std::basic_istream<grampa_char_t> &input) const override
	{ return _subject.match(invert, input); }

	virtual SubjectT* permute(bool const invert, std::basic_istream<grampa_char_t> &input) const override
	{ return _subject.permute(invert, input); }

	virtual unsigned int recursion_depth() const override
	{ return _subject.recursion_depth(); }

	virtual size_t size() const override
	{ return _subject.size(); }

	virtual void add(Item &i)
	{ (dynamic_cast<Constituent&>(_subject)).add(i); }

	virtual SubjectT const& get_subject() const override
	{ return _subject; }

	virtual size_t length() const override
	{ return _subject.length(); }

	virtual bool equals(Item const *const o) const override
	{ return _subject.equals(o); }

	virtual bool is_ambiguous() const override
	{ return _subject.is_ambiguous(); }

};


#endif /* ABSTRACTDECORATOR_H_ */
