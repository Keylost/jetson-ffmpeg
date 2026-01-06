# Copyright (c) 2016-2018, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# - Try to find OpenGLES
# Once done this will define
#  OPENGLES_FOUND - System has OpenGLES
#  OPENGLES_INCLUDE_DIRS - The OpenGLES include directories
#  OPENGLES_LIBRARIES - The libraries needed to use OpenGLES
#  OPENGLES_DEFINITIONS - Compiler switches required for using OpenGLES

find_package(PkgConfig)

find_path(OPENGLES_INCLUDE_DIR GLES3/gl3.h
          HINTS ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/../include)

find_library(OPENGLES_LIBRARY NAMES libGLESv2.so.2)

set(OPENGLES_LIBRARIES ${OPENGLES_LIBRARY})
set(OPENGLES_INCLUDE_DIRS ${OPENGLES_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set OPENGLES_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(OpenGLES DEFAULT_MSG
                                  OPENGLES_LIBRARY OPENGLES_INCLUDE_DIR)

mark_as_advanced(OPENGLES_INCLUDE_DIR OPENGLES_LIBRARY)
