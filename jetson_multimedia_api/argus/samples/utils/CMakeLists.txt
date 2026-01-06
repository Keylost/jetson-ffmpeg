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

cmake_minimum_required (VERSION 2.6)

project(argussampleutils)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")

find_package(Argus COMPONENTS ARGUS_INCLUDE_DIR OPTIONAL_COMPONENTS ARGUS_LIBRARY)
find_package(OpenGLES)
find_package(EGL REQUIRED)
find_package(X11 REQUIRED)
find_package(CUDA)
find_package(NVMMAPI)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(Cairo REQUIRED cairo)

set(SOURCES
    ArgusHelpers.cpp
    CommonOptions.cpp
    EGLGlobal.cpp
    JPEGConsumer.cpp
    NativeBuffer.cpp
    Observed.cpp
    Options.cpp
    RectUtils.cpp
    Thread.cpp
    WindowBase.cpp
    gtk/GuiElement.cpp
    gtk/Window.cpp
    )

if(CUDA_FOUND AND OPENGLES_FOUND)
    set(SOURCES
        ${SOURCES}
        CUDAHelper.cpp
        )
endif(CUDA_FOUND AND OPENGLES_FOUND)

if(OPENGLES_FOUND)
    set(SOURCES
        ${SOURCES}
        GLContext.cpp
        PreviewConsumer.cpp
        )
    include_directories(
        ${OPENGLES_INCLUDE_DIR}
        )
endif(OPENGLES_FOUND)

if(NVMMAPI_FOUND)
    set(SOURCES
        ${SOURCES}
        nvmmapi/NvNativeBuffer.cpp
        )
    include_directories(
        ${NVMMAPI_INCLUDE_DIR}
        )
    add_definitions(
        ${NVMMAPI_DEFINITIONS}
        )
endif(NVMMAPI_FOUND)

include_directories(
    ${ARGUS_INCLUDE_DIR}
    ${EGL_INCLUDE_DIR}
    ${GTK3_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    )

link_directories(
    ${GTK3_LIBRARY_DIRS}
    )

add_definitions(
    ${GTK3_CFLAGS_OTHER}
    )

if(CUDA_FOUND)
    cuda_add_library(${PROJECT_NAME} ${SOURCES})
    target_link_libraries(${PROJECT_NAME}
        ${CUDA_LIBRARIES}
        )
else(CUDA_FOUND)
    add_library(${PROJECT_NAME} STATIC ${SOURCES})
endif(CUDA_FOUND)

target_link_libraries(${PROJECT_NAME}
    pthread
    ${X11_LIBRARIES}
    ${EGL_LIBRARIES}
    ${GTK3_LIBRARIES}
    )

if(OPENGLES_FOUND)
    target_link_libraries(${PROJECT_NAME}
        ${OPENGLES_LIBRARIES}
        )
endif(OPENGLES_FOUND)

if(NVMMAPI_FOUND)
    target_link_libraries(${PROJECT_NAME}
        ${NVMMAPI_LIBRARIES}
        )
endif(NVMMAPI_FOUND)
