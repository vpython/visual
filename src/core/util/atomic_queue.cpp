#include "util/atomic_queue.hpp"
#include "python/gil.hpp"

#include <boost/thread/xtime.hpp>
#include <boost/python/errors.hpp>
#include <iostream>
#include "util/errors.hpp"

namespace cvisual {

void
atomic_queue_impl::push_notify()
{
	empty = false;
	if (waiting)
		ready.notify_all();
}

namespace {
void
xtime_increment( boost::xtime& time, int ms)
{
	assert( ms < 100);
	time.nsec += ms * 1000000;
	if (time.nsec > 1000000000) {
		time.nsec -= 1000000000;
		time.sec += 1;
	}
}
} // !namespace (anon)

void
atomic_queue_impl::py_pop_wait( lock& L)
{
	using python::gil_release;
	using python::gil_lock;

	// The following code is due to CL Cheung, to avoid a deadlock
	// between this routine waiting to regain the gil and the event
	// processing thread waiting to get the barrier lock L. The
	// symptom was Visual not responding after rapid keypresses.
	L.unlock();
	{
		gil_release release;

		// I took the code to call Py_MakePendingCalls() out.  The internet leads
		// me to believe that it is not necessary.  For example, Python time.sleep()
		// doesn't do it.
		if (empty) {
			L.lock();
			while (empty) {
				waiting = true;
				ready.wait(L);
			}
			L.unlock();
		}
		waiting = false;
	}
	L.lock();

}

} // !namespace cvisual
