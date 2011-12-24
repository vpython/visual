import os
import sys

################################################################################
# A set of helper functions that are used later on for various tasks.

def AddDebugFlags(env):
	if ARGUMENTS.has_key("with-debug"):
		if ARGUMENTS['with-debug'] == "none":
			env.Append( DEFINES='NDEBUG', CCFLAGS='-O2')
		elif ARGUMENTS['with-debug'] == "standard":
			env.Append( CCFLAGS=['-O2', '-malign-double'])
		elif ARGUMENTS['with-debug'] == "full":
			env.Append( CCFLAGS='-g', DEFINES="_GLIBCXX_DEBUG")
	elif release:
		env.Append( CCFLAGS=['-O2', '-g', '-malign-double'])
	else:
		env.Append( CCFLAGS=['-O0', '-g'])

def AddThreadingFlags(env):
	env.Append( LIBS='boost_thread')
	if sys.platform == 'win32' and env['CC'] == 'gcc':
		env.Append( CCFLAGS='-mthreads')
	else:
		env.Append( CCFLAGS='-pthread')

################################################################################

# When packaging a release, set this value to true.
release = True

SetOption( "implicit_cache", 1)
EnsurePythonVersion( 2, 2)

# TODO: Add support to build a gch for wrap_gl.hpp, display_kernel.hpp,
# and renderable.hpp
# TODO: Make a configuration section for the following tests:
# existence and suitability of FTGL.
# The FTGL header file problem
# The existence and suitability of Boost.Python
# The existence of gtkmm
# The existence of gtkglextmm
# The existence and suitability of gle
# The particular version of Python to install against.
# The prefix.

# Build a configuration header file.
base = Environment( CCFLAGS=['-pipe'],
	LINKFLAGS=['-Wl,--warn-once'],
	ENV = os.environ,
	CPPPATH=['include', '/usr/include/python2.3'])

# Add flags for debugging symbols and optimization.
AddDebugFlags( base)
# Crank up the warnings
# Crank up the warnings.  Note that I am disabling the warning WRT use of long long,
# since it really is needed in src/gtk2/rate.cpp.
base.Append( CCFLAGS=['-Wall', '-W', '-Wsign-compare', '-Wconversion',
	'-std=c++98', '-Wdisabled-optimization',
	'-pedantic', '-Wno-long-long'] )

core = base.Copy()
# Workaround brain-dead behavior in FTGL's header file layout
# Add compiler flags for threading support.
AddThreadingFlags( core)

core.ParseConfig( 'pkg-config --cflags --libs sigc++-2.0')
core.Append( LIBS=['gle', 'python2.3'])

srcs = [ "src/core/arrow.cpp", 
	"src/core/util/displaylist.cpp",
	"src/core/util/errors.cpp",
	"src/core/util/extent.cpp",
	"src/core/util/lighting.cpp",
	"src/core/util/quadric.cpp",
	"src/core/util/rgba.cpp",
	"src/core/util/texture.cpp",
	"src/core/util/vector.cpp",
	"src/core/util/clipping_plane.cpp",
	"src/core/util/tmatrix.cpp",
	"src/core/util/gl_free.cpp",
	"src/core/util/icososphere.cpp",
	"src/core/axial.cpp",
	"src/core/box.cpp",
	"src/core/cone.cpp",
	"src/core/cylinder.cpp",
	"src/core/ellipsoid.cpp",
	"src/core/pyramid.cpp",
	"src/core/renderable.cpp",
	"src/core/display_kernel.cpp",
	"src/core/mouseobject.cpp",
	"src/core/ring.cpp",
	"src/core/primitive.cpp",
	"src/core/rectangular.cpp",
	"src/core/sphere.cpp",
	"src/core/frame.cpp",
	"src/core/label.cpp",
	"src/core/unused_curve.cpp",
	"src/core/unused_convex.cpp",
	"src/core/unused_faces.cpp" ]

if sys.platform == 'win32':
	srcs.append( 'src/win32/render_surface.cpp')
	srcs.append( 'src/win32/timer.cpp')
	srcs.append( 'src/win32/random_device.cpp')
	srcs.append( 'src/win32/font.cpp')
	# TODO: Write a file_texture.cpp implementation for Windows,
	
	core.Append( LIBS=['opengl32', 'gdi32', 'glu32', 'comctl32', 'crypt32'])
	core.Append( CPPPATH='include/win32')
	libname = 'bin/vpython-core'
else:
	srcs.append( 'src/gtk2/render_surface.cpp')
	srcs.append( 'src/gtk2/text.cpp')
	srcs.append( 'src/gtk2/file_texture.cpp')
	srcs.append( 'src/gtk2/timer.cpp')
	srcs.append( 'src/gtk2/random_device.cpp')
	srcs.append( 'src/gtk2/display.cpp')
	srcs.append( 'src/gtk2/rate.cpp')
	
	core.ParseConfig( 'pkg-config --cflags --libs gtkglextmm-1.2 '
		+ 'fontconfig gthread-2.0 pangomm-1.4 pangoft2')
	core.Append( LIBS=["GL", "GLU"], LINKFLAGS="-Wl,--no-undefined")
	core.Append( CPPPATH='include/gtk2')
	libname='lib/vpython-core'

# Build libvpython-core.dylib
if sys.platform == 'darwin':
	core.SharedLibrary(
		target=target,
		source=source,
		SHLIBSUFFIX = '.dylib',
		SHLINKFLAGS = '$LINKFLAGS -dynamic' )
# Build libvpython-core.{so,dll}
else:
	core.SharedLibrary( 
		target = libname,
		source = srcs )


################################################################################
# Build the test programs.
tests = core.Copy()
tests.Append( LIBS=['vpython-core'] )
if sys.platform == 'win32':
	tests.Append( LINKFLAGS='-mwindows', LIBPATH='bin')
	main = 'src/win32/winmain.cpp'
else:
	tests.Append( LIBPATH='lib')
	main = 'src/gtk2/main.cpp'

def Test( name):
	tests.Program( target='bin/' + name, 
		source=['src/test/' + name + '.cpp', main])

Test('sphere_lod_test')
Test('arrow_transparent_test')
Test('sphere_texture_test')
Test('arrow_scaling_test')
Test('sphere_transparent_test')
Test('texture_sharing_test')
Test('box_test')
Test('cone_test')
Test('cylinder_test')
Test('ring_test')
Test('pyramid_test')
Test('ellipsoid_test')
# Test('psphere_texture_test')
Test('selection_test')
# Test('conference_demo')
Test('curve_test')
Test('label_test')
Test('convex_test')
Test('bounce_test')
if sys.platform != 'win32':
	Test('gtk_style_test')
	
main = "src/gtk2/main.cpp"
tests.Replace( LINKFLAGS="")
Test('object_zsort_bench')
Test('model_zsort_bench')
Test('sincos_matrix_bench')
Test('matrix_inverse_bench')
base.Program( target='bin/sleep_test', source='src/test/sleep_test.cpp')

################################################################################
# Build the extension module.
from vpython_config_helpers import PythonPlugin, SetupPythonConfVars

py = core.Copy()
# SetupPythonConfVars(py)
py.Append( CPPPATH='/usr/include/python2.3', 
	LIBS=['vpython-core', 'boost_python'],
	LIBPATH='lib',
	# Boost.Python's headers produce prodigious amounts of warnings WRT unused
	#   parameters.
	CPPFLAGS=['-Wno-unused', '-DHAVE_CONFIG_H'])

cvisual = PythonPlugin( env=py,
	target='site-packages/cvisual',
	source=['src/python/wrap_display_kernel.cpp',
		'src/python/wrap_vector.cpp',
		'src/python/wrap_rgba.cpp',
		'src/python/wrap_primitive.cpp',
		'src/python/num_util.cpp',
		'src/python/num_util_impl_numeric.cpp',
		'src/python/num_util_impl_numarray.cpp',
		'src/python/slice.cpp',
		'src/python/curve.cpp',
		'src/python/faces.cpp',
		'src/python/convex.cpp',
		'src/python/cvisualmodule.cpp',
		'src/python/wrap_arrayobjects.cpp',
		'src/python/scalar_array.cpp',
		'src/python/vector_array.cpp' ])

# Establish the default target.
Default(cvisual)
