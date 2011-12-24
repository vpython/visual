#ifndef VPYTHON_UTIL_RATE_HPP
#define VPYTHON_UTIL_RATE_HPP

namespace cvisual {

// This function is stateful and allows an application to control its loop
// execution rate.  When calling rate() once per iteration, rate inserts a small
// delay that is calibrated such that the loop will execute at about 'freq'
// iterations per second.
void rate( const double& freq);

} // !namespace cvisual

#endif // !defined VPYTHON_UTIL_RATE_HPP
