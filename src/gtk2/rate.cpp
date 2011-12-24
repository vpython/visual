#include "util/rate.hpp"

#if defined(_WIN32)
	#include "win32/timeval.h"
#else
	#include <sys/time.h>
	#include <time.h>
#endif

#include <math.h>
#include <unistd.h>

#include <stdexcept>

namespace cvisual {
namespace {

struct timeval : ::timeval
{
	// Convert to straight usec.
	operator long long() const
	{
		return tv_sec * 1000000 + tv_usec;
	}
};
	

struct timespec : ::timespec
{
	timespec()
	{
		tv_sec = 0;
		tv_nsec = 0;
	}
	
	// Convert from usec.
	timespec( long long t)
	{
		tv_sec = t / 1000000;
		tv_nsec = t % 1000000 * 1000;
	}
};

// Parts of the rate_timer common to both the OSX and Linux implementations.
class rate_timer
{
 private:
	timeval origin;

 public:
	rate_timer()
	{
		timerclear( &origin);
		gettimeofday( &origin, 0);
	}
	
	// Force the loop to run in 'sec' seconds by inserting the appropriate delay.
	void delay( double sec);
};

void
rate_timer::delay( double _delay)
{
	timeval now;
	timerclear(&now);
	gettimeofday( &now, 0);
	
	// Convert the requested delay into integer units of usec.
	const long long delay = (long long)(_delay * 1e6);
	long long origin = this->origin;
	
	// The amount of time to wait.
	long long wait = delay - ((long long)now - origin);
	if (wait < 0) {
		gettimeofday( &this->origin, 0);
		return;
	}
	// The amout of time remaining when nanosleep returns
	timespec remaining;
#ifdef __APPLE__ // OSX.
	// OSX's nanosleep() is very accurate :)
	timespec sleep_wait( wait);
	nanosleep( &sleep_wait, &remaining);
#else
	// Computation of the requested delay is the same, but the execution differs
	// from OSX.  This is to provide somewhat more accurate timing on Linux, 
	// whose timing is abominable.
	if (wait > 2000) {
		timespec sleep_wait(wait);
		nanosleep( &sleep_wait, &remaining);
	}
	else {
		// Busy wait out the remainder of the time.
		while (wait > 0) {
			sched_yield();
			gettimeofday( &now, 0);
			wait = delay - ((long long)now - origin);
		}
	}
#endif // !defined __APPLE__
	gettimeofday( &this->origin, 0);
}

} // !namespace (unnamed)

void 
rate( const double& freq)
{
	static rate_timer* rt = 0;
	if (!rt)
		rt = new rate_timer();
	
	if (freq <= 0.0)
		throw std::invalid_argument( "Rate must be positive and nonzero.");
	rt->delay( 1.0/freq );
}

} // !namespace cvisual
