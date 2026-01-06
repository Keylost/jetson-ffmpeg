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

#ifndef NATIVE_BUFFER_H
#define NATIVE_BUFFER_H

#include <Argus/Argus.h>

#include "IEGLImageSource.h"

namespace ArgusSamples
{

/**
 * Base NativeBuffer class. Static create method may be used directly rather than platform-specific
 * native type creation methods when generic buffer allocations are sufficient. All native buffer
 * types must support EGLImage creation.
 */
class NativeBuffer : public IEGLImageSource
{
public:
    virtual ~NativeBuffer();

    /**
     * Generic color format that may be used for generic NativeBuffer creation.
     */
    enum ColorFormat
    {
        COLOR_FORMAT_YUV420,
        COLOR_FORMAT_YUV444
    };

    /**
     * Generic NativeBuffer allocator, used when platform format specifics aren't needed.
     * @param[in] size Size of the native buffer.
     * @param[in] colorFormat Color format of the native buffer.
     * @returns A new NativeBuffer, or NULL on error.
     */
    static NativeBuffer* create(const Argus::Size2D<uint32_t>& size,
                                ColorFormat colorFormat = COLOR_FORMAT_YUV420);

    /**
     * Creates an EGLImage with this native buffer's resources as the source.
     * @param[in] eglDisplay The EGLDisplay to create the image with.
     * @returns an EGLImage handle, or EGL_NO_IMAGE on failure.
     */
    virtual EGLImageKHR createEGLImage(EGLDisplay eglDisplay) = 0;

    /**
     * Returns the size of the buffer.
     */
    virtual const Argus::Size2D<uint32_t>& getSize() const { return m_size; }

protected:
    NativeBuffer(const Argus::Size2D<uint32_t>& size);

    Argus::Size2D<uint32_t> m_size;
};


}; // namespace ArgusSamples

#endif // NATIVE_BUFFER_H
