#ifndef VPYTHON_UTIL_RENDER_MANAGER_HPP
#define VPYTHON_UTIL_RENDER_MANAGER_HPP
#pragma once

#include <vector>

namespace cvisual {
	struct render_manager {
		// Called by the platform drivers to paint and swaps all of the given displays,
		// returning the number of seconds to wait before calling this function again.
		// Takes care of a lot of platform-independent policy and implementation, including
		// the tradeoff between frame rate and Python program performance, vertical retrace
		// synchronization, etc.
		static double paint_displays( const std::vector< class display* >&, bool swap_single_threaded = false );
	};
};

#endif
