# This file contains a series of configuration helpers for SConstruct.  It
# should not be run on its own.

import sys
import string
boost_version = ()

def GetBoostVersion(conf, env):
	# Establish's the version of Boost.
	found = 0
	program = """
	#include <boost/version.hpp>
	#include <iostream>
	int	main(void)
	{
		std::cout << BOOST_VERSION / 100000 << " " 
			<< BOOST_VERSION / 100 % 1000 << std::endl;
		return 0;
	}
	"""
	if conf.CheckCXXHeader( header="boost/version.hpp", include_quotes="<>"):
		found = 1
	else:
		for prefix in ["/usr", "/usr/local"]:
			for suffix in ["-1_33", "-1_32", "-1_31", "-1_30"]:
				if conf.CheckCXXHeader( header=prefix + "/boost" + suffix +
						"/boost/version.hpp", include_quotes="<>"):
					env.Append( CPPPATH=prefix + "/boost" + suffix)
					found = 1
					break
	version_info = conf.TryRun( text=program, extension=".cpp")
	if version_info[1] == "":
		print "Could not determine the version of Boost on the system."
		sys.exit(1)
	version_info = string.split(version_info[1])
	boost_version=(int(version_info[0]), int(version_info[1]))
	

def ConfigHeader( env, target, source, defs):
	pass

def AddFTGLDependancy(env):
	pass

def SetupPythonConfVars( env):
	# Modifys a construction environment for building a Python plugin.
	conf = env.Configure()
	GetBoostVersion( env=env, conf=conf)
	# Pick up libpython$PYTHON_VERSION
	if sys.platform == 'win32':
		# TODO: Add support for MSVC's cl.
		libname = "python" + str(sys.version_info[0]) + str(sys.version_info[1])
		if not conf.CheckLib( library=libname, symbol="PyCallable_Check"):
			print "lib" + libname, "must be available to the linker on MS Windows."
			sys.exit(1)
		pass
	else:
		libname = "python" + str(sys.version_info[0]) + "." + str(sys.version_info[1])
		if conf.CheckLib( library=libname, symbol="PyCallable_Check", autoadd=0):
			env.Append( LIBS=libname)
		else:
			print ("WARNING: libpython2.3 was not found.  Assuming symbols "
				+ "will be found in the interpreter executable.")
	# Pick up libboost_python.so
	if not conf.CheckLib( library='boost_python'):
		conf.CheckLib( library="boost_python-gcc-mt-" + 
			str(boost_version[0]) + "_" + str(boost_version[1]))
	if sys.platform == 'linux2':
		env.Append( LINKFLAGS=
			"-Wl,--version-script=src/python/linux-symbols.map")
	# Establish configuration variables to pick up Boost and Python.
	conf.Finish()

def PythonPlugin( env, target=None, source=None):
	# Build a plugin extension to Python on Visual's three supported platforms.
	if sys.platform == 'linux2':
		return env.SharedLibrary( 
			target=target,
			source=source,
			SHLIBPREFIX="")
	elif sys.platform == 'darwin':
		# Ref: http://www.scons.org/cgi-bin/wiki/MacOSX
		return env.SharedLibrary(
			target=target,
			source=source,
			SHLIBPREFIX="",
			SHLIBSUFFIX=".so",
			SHLINKFLAGS="$LINKFLAGS -bundle -flat_namespace -undefined suppress ")
	elif sys.platform == 'win32':
		return env.SharedLibrary(
			target=target,
			source=source)
	else:
		# Fall back to the default for unknown targets.  Yes, I know it is the
		# same as Windows.
		return env.SharedLibrary(
			target=target,
			source=source)
