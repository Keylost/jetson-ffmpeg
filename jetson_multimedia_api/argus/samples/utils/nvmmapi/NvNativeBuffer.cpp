/*
 * Copyright (c) 2017 NVIDIA Corporation.  All rights reserved.
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

#include "nvmmapi/NvNativeBuffer.h"

namespace ArgusSamples
{

    NvNativeBuffer::NvNativeBuffer(const Argus::Size2D<uint32_t>& size)
    : NativeBuffer(size)
    , m_fd(-1)
{
}

NvNativeBuffer::~NvNativeBuffer()
{
    if (m_fd >= 0)
        NvBufferDestroy(m_fd);
}

/* static */
NvNativeBuffer* NvNativeBuffer::create(const Argus::Size2D<uint32_t>& size,
                                       NvBufferColorFormat colorFormat,
                                       NvBufferLayout layout)
{
    NvNativeBuffer* buffer = new NvNativeBuffer(size);
    if (!buffer)
        return NULL;

    NvBufferCreateParams inputParams = {0};

    inputParams.width = size.width();
    inputParams.height = size.height();
    inputParams.layout = layout;
    inputParams.colorFormat = colorFormat;
    inputParams.payloadType = NvBufferPayload_SurfArray;
    inputParams.nvbuf_tag = NvBufferTag_CAMERA;

    if (NvBufferCreateEx(&buffer->m_fd, &inputParams))
    {
        delete buffer;
        return NULL;
    }

    return buffer;
}

EGLImageKHR NvNativeBuffer::createEGLImage(EGLDisplay eglDisplay)
{
    return NvEGLImageFromFd(eglDisplay, m_fd);
}

} // namespace ArgusSamples
