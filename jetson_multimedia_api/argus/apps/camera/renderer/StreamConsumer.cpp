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

#define GL_GLEXT_PROTOTYPES

#include <GLES3/gl31.h>
#include <GLES2/gl2ext.h>

#include <unistd.h>

#include "StreamConsumer.h"
#include "Composer.h"
#include "Error.h"
#include "Util.h" // for TimeValue

namespace ArgusSamples
{

StreamConsumer::StreamConsumer(EGLStreamKHR eglStream)
    : m_initialized(false)
    , m_eglStream(eglStream)
    , m_streamState(EGL_NONE)
    , m_streamTexture(0)
    , m_aspectRatio(1.0f)
{
}

StreamConsumer::~StreamConsumer()
{
    if (!shutdown())
        REPORT_ERROR("Failed to shutdown stream consumer");
}

bool StreamConsumer::initialize()
{
    if (m_initialized)
        return true;

    // Create an external texture and connect it to the stream as a the consumer.
    glGenTextures(1, &m_streamTexture);
    if (m_streamTexture == 0)
        ORIGINATE_ERROR("Failed to create GL texture");

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_streamTexture);
    if (!eglStreamConsumerGLTextureExternalKHR(Composer::getInstance().getEGLDisplay(),
        m_eglStream))
    {
        ORIGINATE_ERROR("Unable to connect GL as consumer");
    }
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

    m_initialized = true;

    return true;
}

bool StreamConsumer::shutdown()
{
    if (m_streamTexture)
    {
        glDeleteTextures(1, &m_streamTexture);
        m_streamTexture = 0;
    }

    m_initialized = false;

    return true;
}

bool StreamConsumer::isEGLStream(EGLStreamKHR eglStream) const
{
    return (m_eglStream == eglStream);
}

uint32_t StreamConsumer::getStreamTextureID() const
{
    return m_streamTexture;
}

bool StreamConsumer::setStreamAspectRatio(float aspectRatio)
{
    m_aspectRatio = aspectRatio;
    return true;
}

float StreamConsumer::getStreamAspectRatio() const
{
    return m_aspectRatio;
}

bool StreamConsumer::acquire(bool *acquiredNewFrame)
{
    if (!m_initialized)
        PROPAGATE_ERROR(initialize());

    const EGLDisplay display = Composer::getInstance().getEGLDisplay();

    // check the stream state
    if (!eglQueryStreamKHR(display, m_eglStream, EGL_STREAM_STATE_KHR, &m_streamState))
        ORIGINATE_ERROR("eglQueryStreamKHR failed (error 0x%04x)", eglGetError());

    if ((m_streamState == EGL_BAD_STREAM_KHR) ||
        (m_streamState == EGL_BAD_STATE_KHR))
    {
        ORIGINATE_ERROR("EGL stream is in bad state (0x%04x)", m_streamState);
    }
    else if (m_streamState == EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR)
    {
        // acquire the new frame
        if (!eglStreamConsumerAcquireKHR(display, m_eglStream))
        {
            // if the acquire failed check if the stream had been disconnected
            const EGLint eglError = eglGetError();
            if (eglError == EGL_BAD_STATE_KHR)
                m_streamState = EGL_BAD_STATE_KHR;
            else
                ORIGINATE_ERROR("Failed to acquire from egl stream (error 0x%04x)", eglError);
        }
        else
        {
            if (acquiredNewFrame)
                *acquiredNewFrame = true;
        }
    }
    else if ((m_streamState == EGL_NONE) ||
             (m_streamState == EGL_STREAM_STATE_CONNECTING_KHR))
    {
        // if the producer is not yet connected just return
    }
    else if ((m_streamState == EGL_STREAM_STATE_EMPTY_KHR) ||
             (m_streamState == EGL_STREAM_STATE_OLD_FRAME_AVAILABLE_KHR))
    {
        // no frame or no new frame, nothing to do
    }
    else if (m_streamState == EGL_STREAM_STATE_DISCONNECTED_KHR)
    {
        // producer had been disconnected, ignore
    }
    else
    {
        ORIGINATE_ERROR("Unhandled EGL stream state (0x%04x)", m_streamState);
    }

    return true;
}

}; // namespace ArgusSamples
