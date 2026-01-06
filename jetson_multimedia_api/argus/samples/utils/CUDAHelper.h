/*
 * Copyright (c) 2016 - 2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CUDAHELPER_H
#define CUDAHELPER_H

#include <cuda.h>
#include <cudaEGL.h>

#ifndef CU_EGL_COLOR_FORMAT_BAYER_RGGB
#define CU_EGL_COLOR_FORMAT_BAYER_RGGB (0x2D)
#define CU_EGL_COLOR_FORMAT_BAYER_BGGR (0x2E)
#define CU_EGL_COLOR_FORMAT_BAYER_GRBG (0x2F)
#define CU_EGL_COLOR_FORMAT_BAYER_GBRG (0x30)
#endif

#ifndef CU_EGL_COLOR_FORMAT_YUV422_PLANAR_ER
#define CU_EGL_COLOR_FORMAT_YUV422_PLANAR_ER     (0x22)
#define CU_EGL_COLOR_FORMAT_YUV420_PLANAR_ER     (0x23)
#define CU_EGL_COLOR_FORMAT_YUV422_SEMIPLANAR_ER (0x25)
#define CU_EGL_COLOR_FORMAT_YUV420_SEMIPLANAR_ER (0x26)
#endif

namespace ArgusSamples
{

/**
 * Returns a readable error string from a given CUResult enum
 *
 * @param [in] cuResult, the enum to convert to string.
 */
const char *getCudaErrorString(const CUresult cuResult);

/**
 * Sets up the CUDA library and opens the CUDA device
 *
 * @param [in][out] the global CUDA context.
 */
bool initCUDA(CUcontext *context);

/**
 * Cleans up the CUDA libraries and closes the CUDA device.
 *
 * @param [in][out] the global CUDA context.
 */
bool cleanupCUDA(CUcontext *context);

/**
 * Checks if the format specified is YUV
 * @param [in] cudaEGLFormat to check the eglColorFormat.
 */
bool isCudaFormatYUV(const CUeglColorFormat cudaEGLFormat);

/**
 * Prints out the CUDA frame information.
 *
 * @param [in] cudaEGLFrame, the frame to display information for.
 */
bool printCUDAEGLFrame(const CUeglFrame &cudaEGLFrame);

} // namespace ArgusSamples

#endif // CUDAHELPER_H
