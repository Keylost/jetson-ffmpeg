===============================================================================
Argus Camera API
===============================================================================

Included Components
-------------------

 - Argus API headers ('include/')
   These headers are required to compile applications against the Argus API.

 - Sample applications ('samples/')
   The included samples demonstrate many features of Argus.

 - Applications ('apps/')
   Includes more complex applications using Argus.

 - Documentation ('docs/')
     - Core Specification (LaTeX document, 'docs/spec')
     - Extensions Specifications ('docs/extensions')
     - Doxygen-generated documentation from the headers ('docs/doxygen')

Building Samples And Apps
=========================

Requirements
------------

- cmake
  sudo apt-get install cmake

- build-essential
  sudo apt-get install build-essential

- pkg-config
  sudo apt-get install pkg-config

- X11
  sudo apt-get install libx11-dev

- GTK3
  sudo apt-get install libgtk-3-dev

- expat
  sudo apt-get install libexpat1-dev

- JPEG
  sudo apt-get install libjpeg-dev

- gstreamer-1.0
  sudo apt-get install libgstreamer1.0-dev

- CUDA
  Samples requiring CUDA (ie. cudaHistogram and syncSensor) will be silently
  ignored during the build process if the CUDA toolkit is not installed.
  The CUDA toolkit is included with the JetPack for L4T development pack, which
  can be downloaded and installed by following the installation steps on the
  NVIDIA developer website:

      https://developer.nvidia.com/embedded/jetpack

  Note that CUDA does not support gcc versions newer than gcc-6, so compiling
  samples that use CUDA will also require the installation of gcc-6:

      sudo apt-get install gcc-6 g++-6
      sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 10
      sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-6 10


Building and Installing
-----------------------

Start in the source directory (e.g. '~/ArgusSDK/').

  1) Create and go into a build directory:
       'mkdir build && cd build'

  1) Create the makefiles:
       'cmake ..'
     Additional options:
       - If CMake cannot find an include path for any dependencies, it may be
         required to provide them explicitly. Eg:
           'cmake -DOPENGLES_INCLUDE_DIR=/path/to/khronos/includes ..'
       - The DISABLE_MULTIPROCESS option may be provided to use the single-process
         Argus implementation (ie. does not require nvargus-daemon service):
           'cmake -DDISABLE_MULTIPROCESS=ON ..'

  2) Build all apps (optional flag -jN for parallel build where N is the job count):
       'make [-jN]'
     Alternatively, build individual executables:
       'make [-jN] argus_openglbox'

  3) Install:
       'sudo make install'
