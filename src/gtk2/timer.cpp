// Copyright (c) 2004 by Jonathan Brandmeyer.
// See the file license.txt for complete license terms.
// See the file authors.txt for a complete list of contributors.

#include "util/timer.hpp"

#include <sys/time.h>
#include <time.h>

#include <cmath>
#include <numeric>

namespace cvisual {
	
timer::timer()
	: start(0), inv_tick_count(0)
{
	start = elapsed();
}

double timer::elapsed() {
	timeval t;
	timerclear(&t);
	gettimeofday( &t, NULL);
	return static_cast<double>(t.tv_sec)
		+ static_cast<double>(t.tv_usec)*1e-6 
		- start;
}

} // !namespace cvisual
