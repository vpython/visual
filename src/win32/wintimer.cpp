// Copyright (c) 2004 by Jonathan Brandmeyer.
// See the file license.txt for complete license terms.
// See the file authors.txt for a complete list of contributors.

#include "util/timer.hpp"

#include <windows.h>

#include <cmath>
#include <numeric>
#include <stdexcept>

namespace cvisual {

timer::timer()
	: start(0)
{
	LARGE_INTEGER freq;
	if (!QueryPerformanceFrequency(&freq))
		throw std::runtime_error( "No high resolution timer." );
	inv_tick_count = 1.0 / static_cast<double>(freq.QuadPart);
	
	start = elapsed();
}

double
timer::elapsed()
{
	LARGE_INTEGER count;
	QueryPerformanceCounter( &count );
	return static_cast<double>(count.QuadPart)*inv_tick_count - start;
}

} // !namespace cvisual
