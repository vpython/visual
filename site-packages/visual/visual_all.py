from vis import version

# Copyright David Scherer and others, see license.txt

import sys as _sys

try:
    from Polygon import Polygon
except:
    pass

visual = _sys.modules['visual']

# _fix_symbols() is called from modules in the package that (unfortunately)
# must execute "from visual import *" because clients expect to be able
# to import them _instead_ of visual.This prevents them from binding
# symbols to other _modules_, which may overwrite symbols imported
# directly from those modules.  (yuck!)
def _fix_symbols( modg ):
    for sym in ( 'controls','factorial' ):
        if sym in modg:
            del modg[sym]
    all = set(modg.keys()) & set(globals().keys())
    return list(all)

# The following manipulations of math and numpy functions is a workaround
# for the problem that in going from Numeric to numpy, the return by numpy
# from e.g. sqrt is numpy.float64, not float, which is not recognized as
# matching float in the operator overloading machinery, including Boost.
# This means that right multiplication scalar*vector is not caught
# and the result is returned as numpy.ndarray instead of vector, which
# can be a big performance hit in vector calculations.

# There is an advantage to this workaround: sqrt(scalar) is much faster
# this way than when using the numpy sqrt, and there is little penalty
# for the numpy sqrt(array).

import math as _math
import numpy as _numpy

from math import *
from numpy import *

for ufunc in ('ceil','cos','cosh','exp','fabs','floor','fmod','frexp',
              'ldexp','log','log10','modf','sin','sinh','sqrt',
              'tan','tanh'):
    def _uf(x,
            numpy = getattr(_numpy,ufunc),
            math = getattr(_math,ufunc),
            mathtypes = (float,int,int)):
        if type(x) in mathtypes: return math(x)
        return numpy(x)
    globals()[ufunc] = _uf

for ufunc in ('hypot',):
    def _uf(x,y,
            numpy = getattr(_numpy,ufunc),
            math = getattr(_math,ufunc),
            mathtypes = (float,int,int)):
        if type(x) in mathtypes and type(y) in mathtypes: return math(x,y)
        return numpy(x,y)
    globals()[ufunc] = _uf

import vis.crayola as color
from vis.cvisual import (vector, mag, mag2, norm, cross, rotate,
                             comp, proj, diff_angle, rate)

# Fix the problem that numpy dot(vector,vector) returns numpy.float64 rather
# than an ordinary float, causing trouble with later vector calculations:
import vis.cvisual as _cvisual
def _dot(x,y,mathtypes = (float,int,int)):
    numpy = getattr(_numpy,'dot')
    cvisual = getattr(_cvisual,'dot')
    if type(x) is vector and type(y) is vector: return cvisual(x,y)
    return numpy(x,y) # arrays rather than Visual vectors
globals()['dot'] = _dot

from vis.primitives import (arrow, cylinder, cone, sphere, box, ring, label,
                               frame, pyramid, ellipsoid, curve, faces, convex, helix,
                               points, text, distant_light, local_light, extrusion)
from vis.ui import display
import vis.materials as materials
import vis.shapes as shapes
import vis.paths as paths 

# Undo side effect of from... import * that puts modules in package namespace
##del ui, crayola, primitives

# Names defined for backward compatibility:
true = True
false = False
crayola = color

# The following ensures that __waitclose will be run
# when we reach the end of the program,
# to permit viewing and navigating the scene.
import atexit as _atexit
_atexit.register(_cvisual.waitclose)

import vis.site_settings

# Construct the default display object.
scene = display()
