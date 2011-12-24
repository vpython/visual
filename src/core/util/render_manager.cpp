#include "util/render_manager.hpp"
#include "util/errors.hpp"
#include <threadpool.hpp>
#include "display.hpp"

#include <boost/python/detail/wrap_python.hpp>

/* No longer used:
#include <boost/python/import.hpp>
*/

namespace cvisual {

/* No longer used:
using boost::python::object;
using boost::python::import;
*/

double render_manager::paint_displays( const std::vector< display* >& displays, bool swap_single_threaded ) {
	// If there are no active displays, poll at a reasonable rate.  The platform driver
	// may turn off polling in this situation, which is fine.
	if (!displays.size()) return .030;

	static timer time;
	static boost::threadpool::pool* swap_thread_pool = NULL;

	// Most of the time spent in paint() will be holding the lock, and most of the time
	// holding the lock is spent in paint().  So we measure this time as an estimate of
	// how long per cycle we are holding the lock.
	double start = time.elapsed();
	for(size_t d=0; d<displays.size(); d++)
		displays[d]->paint();
	double paint = time.elapsed() - start;

	if (swap_single_threaded) {
		for(size_t d=0; d<displays.size(); d++)
			displays[d]->swap();
	} else {
		// Use a thread pool to call SwapBuffers for each display in a separate thread, since
		// at least with some drivers, it can block waiting for vertical retrace
		if (displays.size() > 1) {
			if (!swap_thread_pool)
				swap_thread_pool = new boost::threadpool::pool( displays.size()-1 );
			else if ( swap_thread_pool->size() < displays.size() )
				swap_thread_pool->size_controller().resize( displays.size()-1 );
			
			for(size_t d=1; d<displays.size(); d++)
				swap_thread_pool->schedule( boost::bind( &display::swap, displays[d] ) );
		}
		displays[0]->swap();
		if (displays.size() > 1)
			swap_thread_pool->wait();
	}
	
	double swap = time.elapsed() - (start+paint);
	
	// We want to be holding the lock about half the time, so the next rendering cycle
	// should begin /paint/ seconds after painting finished /swap/ seconds ago.  The minimum
	// of 5ms is to prevent absurd behavior if vertical retrace synchronization is disabled in
	// the driver, and to ensure that we have some time for event handling if painting is instant.
	double interval = std::max(.005, paint - swap);
	if (paint+swap+interval < 0.03) interval = 0.03-paint-swap;
	
	#if 0  // for debugging
	static double lasts = 0.0;
	printf("%0.3f (+%0.3f) %0.3f %0.3f %0.3f\n", start, start-lasts, paint, swap, interval);
	lasts = start;
	#endif

	/* No longer used:
	python::gil_lock gil;
	object handlemouse = import("vis.primitives").attr("handlemouse");
	handlemouse(); // check for mouse events for graph and controls
	*/
	
	return interval;
}

} // namespace cvisual
