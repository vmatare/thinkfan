#ifndef THINKFAN_YAMLCONFIG_H_
#define THINKFAN_YAMLCONFIG_H_

#include "config.h"
#include "thinkfan.h"

#include <vector>
#include <memory>
#include <yaml-cpp/yaml.h>


namespace YAML {

using namespace std;

/**
 * This the glorious wtf_ptr which unifies shared_ptr semantics with
 * the unique_ptr's ability to release(), i.e. relinquish ownership.
 * This weird hack is required since yaml-cpp cannot deal with non-copyable types like
 * std::unique_ptr. I'm sorry.
 */
template<typename T>
class wtf_ptr : public shared_ptr<unique_ptr<T>> {
public:
	using shared_ptr<unique_ptr<T>>::shared_ptr;

	template<typename T1>
	wtf_ptr<T>(wtf_ptr<T1> &&ptr)
		: shared_ptr<unique_ptr<T>>(new unique_ptr<T>(ptr.release()))
	{}

	wtf_ptr()
		: shared_ptr<unique_ptr<T>>()
	{}

	wtf_ptr(T *ptr)
		: shared_ptr<unique_ptr<T>>(new unique_ptr<T>(ptr))
	{}

	auto release()
	{ return this->get()->release(); }

	auto operator -> () const
	{ return this->get()->operator -> (); }

	auto operator * () const
	{ return this->get()->operator * (); }
};


template<typename T, typename... TArgs>
wtf_ptr<T> make_wtf(TArgs... args)
{ return wtf_ptr<T>(new unique_ptr<T>(new T(args...))); }


template<>
struct convert<wtf_ptr<thinkfan::Config>> {
	static bool decode(const Node &node, wtf_ptr<thinkfan::Config> &config);
};



}


#endif // THINKFAN_YAMLCONFIG_H_
