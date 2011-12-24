#include "util/rate.hpp"

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <stdexcept>

namespace cvisual {

namespace {
class rate_timer
{
 long long tics_per_second;
 long long origin;
 
 public:
	rate_timer()
	{
		QueryPerformanceFrequency( (LARGE_INTEGER*)&tics_per_second);
		QueryPerformanceCounter( (LARGE_INTEGER*)&origin);
	}
	
	void delay( double _delay)
	{
		const long long delay = (long long)( _delay * (double)tics_per_second);
		long long now = 0;
		QueryPerformanceCounter( (LARGE_INTEGER*)&now);
		long long wait = delay - (now - origin);
		if (wait < 0) {
			QueryPerformanceCounter( (LARGE_INTEGER*)&origin);
			return;
		}
		// Convert from ticks to milliseconds.
		DWORD ms_wait = wait * 1000 / tics_per_second;
		Sleep( ms_wait);
		// Sleep() usually under-delays by up to 16 ms, so, busy wait out the
		// remainder.  See also src/test/sleep_test.cpp
		QueryPerformanceCounter( (LARGE_INTEGER*)&now);
		wait = delay - (now - origin);
		while (wait > 0) {
			QueryPerformanceCounter( (LARGE_INTEGER*)&now);
			wait = delay - (now - origin);
		}
		QueryPerformanceCounter( (LARGE_INTEGER*)&origin);
	}
};
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
