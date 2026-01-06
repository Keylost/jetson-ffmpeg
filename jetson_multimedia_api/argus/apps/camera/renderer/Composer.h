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

#ifndef COMPOSER_H
#define COMPOSER_H

#include "EGLGlobal.h"

#include <list>

#include "Window.h"
#include "Thread.h"
#include "Mutex.h"

#include "GLContext.h"

namespace ArgusSamples
{

class StreamConsumer;

/**
 * The composer is used to render multiple EGL streams into the windows. The streams are arranged
 * into a regular grid.
 */
class Composer : public Thread, public Window::IResizeObserver
{
public:
    /**
     * Get the composer instance.
     */
    static Composer &getInstance();

    /**
     * Shutdown, free all resources
     */
    bool shutdown();

    /**
     * Bind an EGL stream. A bound and active stream is rendered. Newly bound streams are inactive.
     *
     * @param eglStream [in]
     */
    bool bindStream(EGLStreamKHR eglStream);

    /**
     * Unbind a bound EGL stream.
     *
     * @param eglStream [in]
     */
    bool unbindStream(EGLStreamKHR eglStream);

    /**
     * Set the active state of the stream, only active streams are rendered
     *
     * @param eglStream [in]
     * @param active [in]
     */
    bool setStreamActive(EGLStreamKHR eglStream, bool active);

    /**
     * Set the stream aspect ratio
     *
     * @param eglStream [in]
     * @param aspectRatio [in] aspect ration of the images transported by the stream
     */
    bool setStreamAspectRatio(EGLStreamKHR eglStream, float aspectRatio);

    /**
     * Get the EGL display
     */
    EGLDisplay getEGLDisplay()
    {
        if (initialize())
            return m_display.get();

        return EGL_NO_DISPLAY;
    }

private:
    Composer();
    ~Composer();

    // this is a singleton, hide copy constructor etc.
    Composer(const Composer&);
    Composer& operator=(const Composer&);

    bool initialize();

    /** @name Thread methods */
    /**@{*/
    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();
    /**@}*/

    /** @name IResizeObserver methods */
    /**@{*/
    virtual bool onResize(uint32_t width, uint32_t height);
    /**@}*/

    bool renderStreams(uint32_t activeStreams);

    bool m_initialized;         ///< set if initialized

    EGLDisplayHolder m_display; ///< EGL display

    GLContext m_context;        ///< GL context
    uint32_t m_program;         ///< program to render streams
    uint32_t m_vbo;             ///< vertex buffer object
    uint32_t m_windowWidth;     ///< window width
    uint32_t m_windowHeight;    ///< window height
    float m_windowAspectRatio;  ///< window aspect ratio

    Mutex m_mutex;              ///< to protect access to the stream array

    /**
     * Each bound EGL stream has a stream consumer and can be active or inactive.
     */
    class Stream
    {
    public:
        explicit Stream(StreamConsumer *consumer)
            : m_consumer(consumer)
            , m_active(false)
            , m_shutdown(false)
        {
        }

        StreamConsumer *m_consumer; ///< the stream consumer
        bool m_active;              ///< if set then the stream is active and rendered
        bool m_shutdown;            ///< shutdown and remove the stream
    };

    typedef std::list<Stream> StreamList;   ///< a list of streams
    StreamList m_streams;           ///< the list of composed streams
};

}; // namespace ArgusSamples

#endif // COMPOSER_H
