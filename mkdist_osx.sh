#!/bin/bash
# Run in a little sandbox alongside the source package
cd ..
mkdir -p buildvpython3.0.3
cd buildvpython3.0.3

# Untar and build GCC
if test ! -e /usr/local/bin/g++-3.3.4 ; then
echo "Downloading GCC from the web."
wget http://mirrors.rcn.net/pub/sourceware/gcc/releases/gcc-3.3.4/gcc-3.3.4.tar.bz2
echo "Building GCC 3.3.4 from source."
tar -xjf gcc-3.3.4.tar.bz2
mkdir buildgcc
cd buildgcc
../gcc-3.3.4/configure --enable-version-specific-runtime-libs --enable-threads=posix \
    --disable-shared --enable-static --program-suffix=-3.3.4 --enable-languages=c,c++
make bootstrap
echo "Installing GCC 3.3.4 into /usr/local with sudo, you will need your password here."
sudo make install
cd ..
fi

# Untar and build Boost
if test ! -e /usr/local/lib/libboost_python.dylib ; then
echo "Downloading Boost from the web."
wget http://aleron.dl.sourceforge.net/sourceforge/boost/boost_1_31_0.tar.bz2
echo "Building Boost.Python from source."
tar -xjf boost_1_31_0.tar.bz2
mkdir buildboost1.31.0
cp ../visual-3.0.2/Boost_OSX_Makefile.mak buildboost1.31.0/Makefile
cd buildboost1.31.0
make all
echo "Copying libboost_python.dylib into /usr/local/lib with sudo, you may need your password here."
sudo cp libboost_python.dylib /usr/local/lib
cd ..
fi

# configure and build VPython
echo "Building VPython from source."
rm -rf buildvisual
mkdir buildvisual
cd buildvisual
export CPPFLAGS="-I/sw/include -I../../boost_1_31_0"
export CXXFLAGS="-02 -finline-functions"
export CXX="/usr/local/bin/g++-3.3.4 -pipe"
export CC="/usr/local/bin/gcc-3.3.4 -pipe"
export PYTHON=/sw/bin/python2.3
../../visual-3.0.3/configure --prefix=/sw
make
echo "Installing VPython into /sw with sudo, you may need your password here."
sudo make install
cd ..

# Build a redistributable package
tar -czf vpython-osx-3.0.3.tar.gz /sw/lib/python2.3/site-packages/cvisualmodule.so \
    /sw/lib/python2.3/site-packages/visual/ /usr/local/lib/libboost_python.dylib

# Done.
