#ifndef VPYTHON_UTIL_THREAD_HPP
#define VPYTHON_UTIL_THREAD_HPP

// Copyright (c) 2000, 2001, 2002, 2003 by David Scherer and others.
// Copyright (c) 2004 by Jonathan Brandmeyer and others.
// See the file license.txt for complete license terms.
// See the file authors.txt for a complete list of contributors.

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <cassert>

#include "python/gil.hpp"

namespace cvisual {

#ifdef NDEBUG
 using boost::mutex;
 typedef mutex::scoped_lock lock;
 
 inline void
 assert_locked( const mutex&)
 {
     // Empty.
 }
 
 inline void
 assert_unlocked( const mutex&)
 {
     // Empty.
 }
 
#else
 typedef boost::try_mutex mutex;
 typedef mutex::scoped_lock lock;
 
 inline void
 assert_locked( mutex& m)
 {
     try {
         mutex::scoped_try_lock L(m);
     }
     catch (boost::lock_error&) {
        return;
     }
     bool mutex_locked = false;
     assert( mutex_locked == true);
 }
 
 inline void
 assert_unlocked( mutex& m)
 {
     try {
         mutex::scoped_try_lock L(m);
     }
     catch (boost::lock_error&) {
        bool mutex_locked = true;
        assert( mutex_locked == false);
     }
     return;
 }
#endif

// using boost::condition;

class condition : public boost::condition
{
 public:
	template <typename Lock>
	void py_wait( Lock& L)
	{
		python::gil_release R;
		boost::condition::wait(L);
	}
};

} // !namesapce cvisual

#endif // !defined VPYTHON_UTIL_THREAD_HPP
