/*
 * Copyright (c) 2017, NVIDIA Corporation.  All rights reserved.
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

#ifndef NV_NATIVE_BUFFER_H
#define NV_NATIVE_BUFFER_H

#include "NativeBuffer.h"

#include <nvbuf_utils.h>

namespace ArgusSamples
{

/**
 * NativeBuffer type created from an NvBuffer
 */
class NvNativeBuffer : public NativeBuffer
{
public:
    /**
     * Destructor will free the NvBuffer owned by the buffer.
     */
    virtual ~NvNativeBuffer();

    /**
     * Creates and returns a new NvNativeBuffer.
     * @param[in] size Size of the buffer to allocate.
     * @param[in] colorFormat Format of the buffer.
     * @param[in] layout Layout of the buffer.
     * @returns A new NvNativeBuffer, NULL on error.
     */
    static NvNativeBuffer* create(const Argus::Size2D<uint32_t>& size,
                                  NvBufferColorFormat colorFormat,
                                  NvBufferLayout layout = NvBufferLayout_Pitch);

    /**
     * Creates an EGLImage from the NvNativeBuffer.
     * @param[in] eglDisplay The EGLDisplay to create the image with.
     * @returns an EGLImage handle, or EGL_NO_IMAGE on failure.
     */
    virtual EGLImageKHR createEGLImage(EGLDisplay eglDisplay);

protected:
    NvNativeBuffer(const Argus::Size2D<uint32_t>& size);

    int m_fd;
};

} // namespace ArgusSamples

#endif
