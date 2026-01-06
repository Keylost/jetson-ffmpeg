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

# - Try to find EGL
# Once done this will define
#  EGL_FOUND - System has EGL
#  EGL_INCLUDE_DIRS - The EGL include directories
#  EGL_LIBRARIES - The libraries needed to use EGL
#  EGL_DEFINITIONS - Compiler switches required for using EGL

find_package(PkgConfig)

find_path(EGL_INCLUDE_DIR EGL/egl.h
          HINTS ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/../include)

find_library(EGL_LIBRARY NAMES EGL)

set(EGL_LIBRARIES ${EGL_LIBRARY})
set(EGL_INCLUDE_DIRS ${EGL_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set EGL_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(EGL DEFAULT_MSG
                                  EGL_LIBRARY EGL_INCLUDE_DIR)

mark_as_advanced(EGL_INCLUDE_DIR EGL_LIBRARY)
