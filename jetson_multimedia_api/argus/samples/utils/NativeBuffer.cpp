/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
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

#include "NativeBuffer.h"

#if defined(ANDROID)
#include "android/NvRmSurfaceBuffer.h"
#elif defined(NVMMAPI_SUPPORTED)
#include "nvmmapi/NvNativeBuffer.h"
#endif

namespace ArgusSamples
{

NativeBuffer::NativeBuffer(const Argus::Size2D<uint32_t>& size)
    : m_size(size)
{
}

NativeBuffer::~NativeBuffer()
{
}

/* static */
NativeBuffer* NativeBuffer::create(const Argus::Size2D<uint32_t>& size, ColorFormat colorFormat)
{
    // Currently only support YUV420 and YUV444 generic format
    if (colorFormat == COLOR_FORMAT_YUV420)
#if defined(ANDROID)
        return NvRmSurfaceBuffer::create(size, NvColorFormat_U8_V8);
#elif defined(NVMMAPI_SUPPORTED)
        return NvNativeBuffer::create(size, NvBufferColorFormat_NV12);
#else
        return NULL;
#endif

    else if (colorFormat == COLOR_FORMAT_YUV444)
#if defined(NVMMAPI_SUPPORTED)
        return NvNativeBuffer::create(size, NvBufferColorFormat_NV24);
#else
        return NULL;
#endif

    else
    {
        printf("Only YUV420 and YUV444 Semiplanar formats are supported");
        return NULL;
    }

}

}; // namespace ArgusSamples
