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

#include "CUDAHelper.h"
#include "Error.h"

#include <string.h>
#include <stdlib.h>

namespace ArgusSamples
{
const char *getCudaErrorString(CUresult cuResult)
{
    const char *errorString;
    cuGetErrorString(cuResult, &errorString);

    return errorString;
}

// Initialize CUDA
bool initCUDA(CUcontext *context)
{
    printf("Initializing CUDA\n");

    if (!context)
        ORIGINATE_ERROR("Context NULL");

    CUresult cuResult = cuInit(0);
    if (cuResult != CUDA_SUCCESS)
    {
        ORIGINATE_ERROR("Unable to initialize the CUDA driver API (CUresult %s)",
            getCudaErrorString(cuResult));
    }

    int cudaDeviceCount = 0;
    cuResult = cuDeviceGetCount(&cudaDeviceCount);
    if (cuResult != CUDA_SUCCESS)
    {
        ORIGINATE_ERROR("Unable to get the number of compute-capable devices (CUresult %s)",
            getCudaErrorString(cuResult));
    }
    if (cudaDeviceCount == 0)
        ORIGINATE_ERROR("Did not find any compute-capable devices");

    CUdevice cudaDevice;
    cuResult = cuDeviceGet(&cudaDevice, 0);
    if (cuResult != CUDA_SUCCESS)
    {
        ORIGINATE_ERROR("Unable to get a handle to a compute device (CUresult %s)",
            getCudaErrorString(cuResult));
    }

    cuResult = cuCtxCreate(context, 0, cudaDevice);
    if (cuResult != CUDA_SUCCESS)
    {
        ORIGINATE_ERROR("Unable to initialize the CUDA driver API (CUresult %s)",
            getCudaErrorString(cuResult));
    }

    return true;
}

bool cleanupCUDA(CUcontext *context)
{
    if (!context)
        ORIGINATE_ERROR("Context NULL");

    CUresult cuResult = cuCtxDestroy(*context);
    if (cuResult != CUDA_SUCCESS)
    {
        ORIGINATE_ERROR("Unable to destroy the CUDA context (CUresult %s)",
            getCudaErrorString(cuResult));
    }
    *context = 0;

    return true;
}

bool isCudaFormatYUV(const CUeglColorFormat cudaEGLFormat)
{
    if ((cudaEGLFormat == CU_EGL_COLOR_FORMAT_YUV420_PLANAR) ||
        (cudaEGLFormat == CU_EGL_COLOR_FORMAT_YUV420_SEMIPLANAR) ||
        (cudaEGLFormat == CU_EGL_COLOR_FORMAT_YUV422_PLANAR) ||
        (cudaEGLFormat == CU_EGL_COLOR_FORMAT_YUV422_SEMIPLANAR) ||
        (cudaEGLFormat == CU_EGL_COLOR_FORMAT_YUV420_PLANAR_ER) ||
        (cudaEGLFormat == CU_EGL_COLOR_FORMAT_YUV420_SEMIPLANAR_ER) ||
        (cudaEGLFormat == CU_EGL_COLOR_FORMAT_YUV422_PLANAR_ER) ||
        (cudaEGLFormat == CU_EGL_COLOR_FORMAT_YUV422_SEMIPLANAR_ER))
    {
        return true;
    }

    return false;
}

bool printCUDAEGLFrame(const CUeglFrame &cudaEGLFrame)
{
    printf("CUeglFrame:\n");
    printf(" width: %d\n", cudaEGLFrame.width);
    printf(" height: %d\n", cudaEGLFrame.height);
    printf(" depth: %d\n", cudaEGLFrame.depth);
    printf(" pitch: %d\n", cudaEGLFrame.pitch);
    printf(" planeCount: %d\n", cudaEGLFrame.planeCount);
    printf(" numChannels: %d\n", cudaEGLFrame.numChannels);
    const char *frameTypeString = NULL;
    switch (cudaEGLFrame.frameType)
    {
    case CU_EGL_FRAME_TYPE_ARRAY:
        frameTypeString = "array";
        break;
    case CU_EGL_FRAME_TYPE_PITCH:
        frameTypeString = "pitch";
        break;
    default:
        ORIGINATE_ERROR("Unknown frame type %d", cudaEGLFrame.frameType);
    }
    printf(" frameType: %s\n", frameTypeString);
    const char *colorFormatString = NULL;
    switch (cudaEGLFrame.eglColorFormat)
    {
    case CU_EGL_COLOR_FORMAT_YUV420_PLANAR:
        colorFormatString = "YUV420 planar";
        break;
    case CU_EGL_COLOR_FORMAT_YUV420_SEMIPLANAR:
        colorFormatString = "YUV420 semi-planar";
        break;
    case CU_EGL_COLOR_FORMAT_YUV422_PLANAR:
        colorFormatString = "YUV422 planar";
        break;
    case CU_EGL_COLOR_FORMAT_YUV422_SEMIPLANAR:
        colorFormatString = "YUV422 semi-planar";
        break;
    case CU_EGL_COLOR_FORMAT_YUV420_PLANAR_ER:
        colorFormatString = "YUV420 planar ER";
        break;
    case CU_EGL_COLOR_FORMAT_YUV420_SEMIPLANAR_ER:
        colorFormatString = "YUV420 semi-planar ER";
        break;
    case CU_EGL_COLOR_FORMAT_YUV422_PLANAR_ER:
        colorFormatString = "YUV422 planar ER";
        break;
    case CU_EGL_COLOR_FORMAT_YUV422_SEMIPLANAR_ER:
        colorFormatString = "YUV422 semi-planar ER";
        break;
    case CU_EGL_COLOR_FORMAT_RGB:
        colorFormatString = "RGB";
        break;
    case CU_EGL_COLOR_FORMAT_BGR:
        colorFormatString = "BGR";
        break;
    case CU_EGL_COLOR_FORMAT_ARGB:
        colorFormatString = "ARGB";
        break;
    case CU_EGL_COLOR_FORMAT_RGBA:
        colorFormatString = "RGBA";
    case CU_EGL_COLOR_FORMAT_BAYER_RGGB:
        colorFormatString = "S16 Bayer RGGB";
        break;
    case CU_EGL_COLOR_FORMAT_BAYER_BGGR:
        colorFormatString = "S16 Bayer BGGR";
        break;
    case CU_EGL_COLOR_FORMAT_BAYER_GRBG:
        colorFormatString = "S16 Bayer GRBG";
        break;
    case CU_EGL_COLOR_FORMAT_BAYER_GBRG:
        colorFormatString = "S16 Bayer GBRG";
        break;
    default:
        ORIGINATE_ERROR("Unknown color format %d", cudaEGLFrame.eglColorFormat);
    }
    printf(" colorFormat: %s\n", colorFormatString);
    const char *cuFormatString = NULL;
    switch (cudaEGLFrame.cuFormat)
    {
    case CU_AD_FORMAT_UNSIGNED_INT8:
        cuFormatString = "uint8";
        break;
    case CU_AD_FORMAT_UNSIGNED_INT16:
        cuFormatString = "uint16";
        break;
    case CU_AD_FORMAT_UNSIGNED_INT32:
        cuFormatString = "uint32";
        break;
    case CU_AD_FORMAT_SIGNED_INT8:
        cuFormatString = "int8";
        break;
    case CU_AD_FORMAT_SIGNED_INT16:
        cuFormatString = "int16";
        break;
    case CU_AD_FORMAT_SIGNED_INT32:
        cuFormatString = "int32";
        break;
    case CU_AD_FORMAT_HALF:
        cuFormatString = "float16";
        break;
    case CU_AD_FORMAT_FLOAT:
        cuFormatString = "float32";
        break;
    default:
        ORIGINATE_ERROR("Unknown cuFormat %d", cudaEGLFrame.cuFormat);
    }
    printf(" cuFormat: %s\n", cuFormatString);

    return true;
}

} // namespace ArgusSamples
