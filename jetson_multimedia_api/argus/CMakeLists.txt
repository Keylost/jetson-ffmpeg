# Copyright (c) 2016-2022, NVIDIA CORPORATION. All rights reserved.
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

add_subdirectory(apps/camera)
add_subdirectory(samples/bayerAverageMap)
add_subdirectory(samples/cudaBayerDemosaic)
add_subdirectory(samples/cudaHistogram)
add_subdirectory(samples/denoise)
add_subdirectory(samples/eglImage)
add_subdirectory(samples/gstVideoEncode)
add_subdirectory(samples/histogram)
add_subdirectory(samples/l4t_droid)
add_subdirectory(samples/l4tdroid_App)
add_subdirectory(samples/multiSensor)
add_subdirectory(samples/multiStream)
add_subdirectory(samples/oneShot)
add_subdirectory(samples/openglBox)
add_subdirectory(samples/rawBayerOutput)
add_subdirectory(samples/syncSensor)
add_subdirectory(samples/syncStereo)
add_subdirectory(samples/yuvJpeg)
add_subdirectory(samples/userAutoExposure)
add_subdirectory(samples/userAutoWhiteBalance)
add_subdirectory(samples/utils)
add_subdirectory(samples/yuvOneShot)


include(CPack)
