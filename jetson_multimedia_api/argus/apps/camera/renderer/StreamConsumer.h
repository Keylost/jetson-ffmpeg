/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION. All rights reserved.
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

#ifndef STREAM_CONSUMER_H
#define STREAM_CONSUMER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace ArgusSamples
{

/**
 * The stream consumer is connecting to a EGL stream and consumes the frames into a GL texture.
 */
class StreamConsumer
{
public:
    explicit StreamConsumer(EGLStreamKHR eglStream);
    ~StreamConsumer();

    bool initialize();
    bool shutdown();

    bool isEGLStream(EGLStreamKHR eglStream) const;
    uint32_t getStreamTextureID() const;

    bool setStreamAspectRatio(float aspectRatio);
    float getStreamAspectRatio() const;

    /**
     * @returns the cached stream state
     */
    EGLint getStreamState() const
    {
        return m_streamState;
    }

    /**
     * Check the stream state and acquire a new frame if available
     *
     * @param acquiredNewFrame [out] set to true if a new frame had been acquired
     */
    bool acquire(bool *acquiredNewFrame);

private:
    bool m_initialized;
    EGLStreamKHR m_eglStream;
    EGLint m_streamState;       ///< cached stream state
    uint32_t m_streamTexture;
    float m_aspectRatio;        ///< aspect ration of the images transported by the stream

    /**
     * Hide default constructor
     */
    StreamConsumer();
};

}; // namespace ArgusSamples

#endif // STREAM_CONSUMER_H
