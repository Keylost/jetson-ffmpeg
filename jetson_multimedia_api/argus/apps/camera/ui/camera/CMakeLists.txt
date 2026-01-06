# Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
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

project(argus_camera)

# begin generate source
set(GEN_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/cameraBuilder.glade)
set(GEN_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cameraBuilder.h)

add_custom_command(
   OUTPUT ${GEN_OUTPUT}
   COMMAND xxd -i < ${GEN_INPUT} > ${GEN_OUTPUT}
   DEPENDS ${GEN_INPUT}
)
#end generate source

set(SOURCES
    Main.cpp
    ${GEN_OUTPUT}
    )

include_directories(
    ${ARGUS_INCLUDE_DIR}
    ${EGL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/apps/camera/ui/capture
    ${CMAKE_SOURCE_DIR}/apps/camera/ui/common
    ${CMAKE_SOURCE_DIR}/apps/camera/ui/gallery
    ${CMAKE_SOURCE_DIR}/apps/camera/ui/multiExposure
    ${CMAKE_SOURCE_DIR}/apps/camera/ui/multiSession
    ${CMAKE_SOURCE_DIR}/apps/camera/ui/video
    ${CMAKE_SOURCE_DIR}/apps/camera/common
    ${CMAKE_SOURCE_DIR}/apps/camera/modules
    ${CMAKE_SOURCE_DIR}/apps/camera/renderer
    ${CMAKE_SOURCE_DIR}/samples/utils
    ${CMAKE_CURRENT_BINARY_DIR}
    )

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    arguscamerauicapture
    arguscamerauicommon
    arguscamerauigallery
    arguscamerauimultiexposure
    arguscamerauimultisession
    arguscamerauivideo
    arguscameracommon
    arguscameramodules
    arguscamerarenderer
    argussampleutils
    )

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
