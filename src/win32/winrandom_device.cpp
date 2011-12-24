/* boost random_device.cpp implementation
 *
 * Copyright Jens Maurer 2000
 * Permission to use, copy, modify, sell, and distribute this software
 * is hereby granted without fee provided that the above copyright notice
 * appears in all copies and that both that copyright notice and this
 * permission notice appear in supporting documentation,
 *
 * Jens Maurer makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * $Id: winrandom_device.cpp,v 1.5 2008/04/07 19:22:00 bsherwood Exp $
 *
 */

/* Changes:
 *    Replace impl with a version that uses the MS Crypto API to generate random
 *       numbers for use on MS Windows. -JDB
 *
 */

#include <boost/nondet_random.hpp>
#include <string>
#include <cassert>
#include <stdexcept>


#ifndef BOOST_NO_INCLASS_MEMBER_INITIALIZATION
//  A definition is required even for integral static constants
const bool boost::random_device::has_fixed_range;
const boost::random_device::result_type boost::random_device::min_value;
const boost::random_device::result_type boost::random_device::max_value;
#endif

#define WIN32_LEAN_AND_MEAN 1
#ifdef _MSC_VER
    #define NOMINMAX
#endif

#include <windows.h>
#include <wincrypt.h>

const char* const boost::random_device::default_token = "";

class boost::random_device::impl
{
 public:
	impl()
		: handle(0)
	{
		if (!CryptAcquireContext( &handle, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
			error();
	}

	~impl()
	{
		CryptReleaseContext( handle, 0);
	}

	unsigned int next()
	{
		unsigned int ret = 0;
		CryptGenRandom( handle, sizeof(unsigned int), (BYTE*)&ret);
		assert(ret != 0);
		return ret;
	}

 private:
	HCRYPTPROV handle;

	void error()
	{
		throw std::runtime_error( "CryptAquireContext() failed");
	}
};

boost::random_device::random_device(const std::string&)
  : pimpl(new impl())
{
  assert(std::numeric_limits<result_type>::max() == max_value);
}

boost::random_device::~random_device()
{
  // the complete class impl is now visible, so we're safe
  // (see comment in random.hpp)
  delete pimpl;
}

double boost::random_device::entropy() const
{
  return 10;
}

unsigned int boost::random_device::operator()()
{
  return pimpl->next();
}
