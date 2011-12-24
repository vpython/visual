#ifndef VPYTHON_UTIL_TIMER_HPP
#define VPYTHON_UTIL_TIMER_HPP

// Copyright (c) 2004 by Jonathan Brandmeyer and others.
// See the file license.txt for complete license terms.
// See the file authors.txt for a complete list of contributors.

namespace cvisual {

// On this platform we just use Glib::Timer, which has the same interface 
// as timer.  This file should override vpython-core2/include/util/timer.hpp.

#include <gtkmm/main.h>

typedef Glib::Timer render_timer;

} // !namespace cvisual

#endif // !defined VPYTHON_UTIL_TIMER_HPP
