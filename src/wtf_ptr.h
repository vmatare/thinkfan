#pragma once

#include "thinkfan.h"


namespace thinkfan {

/**
 * This is the glorious wtf_ptr which unifies shared_ptr semantics with
 * the unique_ptr's ability to release(), i.e. relinquish ownership.
 * This weird hack is required since yaml-cpp cannot deal with non-copyable types like
 * unique_ptr. I'm sorry.
 */
template<typename T>
class wtf_ptr : public shared_ptr<unique_ptr<T>>, public std::enable_shared_from_this<unique_ptr<T>> {
public:
	using shared_ptr<unique_ptr<T>>::shared_ptr;

	template<typename T1>
	wtf_ptr(wtf_ptr<T1> &&ptr)
		: shared_ptr<unique_ptr<T>>(new unique_ptr<T>(ptr.release()))
	{}

	wtf_ptr()
		: shared_ptr<unique_ptr<T>>()
	{}

	wtf_ptr(T *ptr)
		: shared_ptr<unique_ptr<T>>(new unique_ptr<T>(ptr))
	{}

	auto release()
	{ return *this ? this->get()->release() : nullptr; }

	auto operator -> () const
	{ return this->get()->operator -> (); }

	auto operator * () const
	{ return this->get()->operator * (); }

	template<typename T1>
	wtf_ptr<T> &operator = (const wtf_ptr<T1>& ptr) noexcept
	{ return this->shared_ptr<unique_ptr<T>>::operator = (ptr); }
};


template<typename T, typename... TArgs>
wtf_ptr<T> make_wtf(TArgs... args)
{ return wtf_ptr<T>(new unique_ptr<T>(new T(args...))); }


} // namespace thinkfan
