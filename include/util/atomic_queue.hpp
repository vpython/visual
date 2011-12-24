#ifndef VPYTHON_UTIL_ATOMIC_QUEUE_HPP
#define VPYTHON_UTIL_ATOMIC_QUEUE_HPP

#include "util/thread.hpp"
#include <queue>

namespace cvisual {
	
// This class exists to separate out code that is not templated (and therefore
// doesn't need to be in a header file) from code that is.  This prevents
// picking up the whole Python runtine throughout all of cvisual, thus making
// compilation faster.
class atomic_queue_impl
{
 protected:
	volatile bool waiting;
	volatile bool empty;
	condition ready;
	mutable mutex barrier;
	
	atomic_queue_impl()
		: waiting(false), empty(true)
	{}
	
	void push_notify();
	void py_pop_wait( lock& L);
};

template <typename T>
class atomic_queue : private atomic_queue_impl
{
 private:
	std::queue<T> data;
	
	/**
	 * Precondition - the buggar is locked, at least one item is in data
	 */
	T pop_impl()
	{
		T ret = data.front();
		data.pop();
		if (data.empty())
			empty = true;
		return ret;
	}

 public:
	atomic_queue() {}
	
	void push( const T& item)
	{
		lock L(barrier);
		data.push( item);
		push_notify();
	}

	// It's assumed that the caller of py_pop() holds the Python GIL
	T py_pop()
	{
		lock L(barrier);
		py_pop_wait( L);
		return pop_impl();
	}
	
	size_t size() const
	{
		lock L(barrier);
		return data.size();
	}
	
	void clear()
	{
		lock L(barrier);
		while (!data.empty())
			data.pop();
		empty = true;
	}
};

} // !namespace cvisual

#endif // !defined VPYTHON_UTIL_ATOMIC_QUEUE_HPP
