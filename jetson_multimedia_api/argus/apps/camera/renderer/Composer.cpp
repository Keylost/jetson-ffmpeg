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

#include <math.h>

#include "Error.h"
#include "UniquePointer.h"
#include "InitOnce.h"

#include "Composer.h"
#include "Window.h"
#include "StreamConsumer.h"
#include "PerfTracker.h"

namespace ArgusSamples
{

Composer::Composer()
    : m_initialized(false)
    , m_program(0)
    , m_vbo(0)
    , m_windowWidth(0)
    , m_windowHeight(0)
    , m_windowAspectRatio(1.0f)
{
}

Composer::~Composer()
{
    if (!shutdown())
        REPORT_ERROR("Failed to shutdown composer");
}

Composer &Composer::getInstance()
{
    static InitOnce initOnce;
    static Composer instance;

    if (initOnce.begin())
    {
        if (instance.initialize())
        {
            initOnce.complete();
        }
        else
        {
            initOnce.failed();
            REPORT_ERROR("Initalization failed");
        }
    }

    return instance;
}

bool Composer::initialize()
{
    if (m_initialized)
        return true;

    Window &window = Window::getInstance();

    PROPAGATE_ERROR(m_display.initialize(window.getEGLNativeDisplay()));

    PROPAGATE_ERROR(m_mutex.initialize());

    // initialize the window size
    PROPAGATE_ERROR(onResize(window.getWidth(), window.getHeight()));

    // and register as observer for size changes
    PROPAGATE_ERROR(window.registerObserver(this));

    PROPAGATE_ERROR(Thread::initialize());
    PROPAGATE_ERROR(Thread::waitRunning());

    m_initialized = true;

    return true;
}

bool Composer::shutdown()
{
    if (!m_initialized)
        return true;

    PROPAGATE_ERROR_CONTINUE(Window::getInstance().unregisterObserver(this));

    // request shutdown of the thread
    PROPAGATE_ERROR_CONTINUE(Thread::requestShutdown());

    PROPAGATE_ERROR_CONTINUE(Thread::shutdown());

    PROPAGATE_ERROR_CONTINUE(m_display.cleanup());

    m_initialized = false;

    return true;
}

bool Composer::bindStream(EGLStreamKHR eglStream)
{
    if (eglStream == EGL_NO_STREAM_KHR)
        ORIGINATE_ERROR("Invalid stream");

    PROPAGATE_ERROR(initialize());

    UniquePointer<StreamConsumer> streamConsumer(new StreamConsumer(eglStream));
    if (!streamConsumer)
        ORIGINATE_ERROR("Out of memory");

    // add the new stream consumer to the stream list
    {
        ScopedMutex sm(m_mutex);
        PROPAGATE_ERROR(sm.expectLocked());

        m_streams.push_back(Stream(streamConsumer.get()));
    }

    // wait until the stream is connected
    while (streamConsumer->getStreamState() != EGL_STREAM_STATE_CONNECTING_KHR)
        usleep(1000);
    streamConsumer.release();

    return true;
}

bool Composer::unbindStream(EGLStreamKHR eglStream)
{
    ScopedMutex sm(m_mutex);
    PROPAGATE_ERROR(sm.expectLocked());

    for (StreamList::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
    {
        if (it->m_consumer->isEGLStream(eglStream))
        {
            // set the shutdown flag, the composer thread will do the actual shutdown
            it->m_shutdown = true;
            return true;
        }
    }

    ORIGINATE_ERROR("Stream was not bound");

    return true;
}

bool Composer::setStreamActive(EGLStreamKHR eglStream, bool active)
{
    ScopedMutex sm(m_mutex);
    PROPAGATE_ERROR(sm.expectLocked());

    for (StreamList::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
    {
       if (it->m_consumer->isEGLStream(eglStream))
       {
            it->m_active = active;
            return true;
        }
    }

    ORIGINATE_ERROR("Stream was not bound");

    return true;
}

bool Composer::setStreamAspectRatio(EGLStreamKHR eglStream, float aspectRatio)
{
    ScopedMutex sm(m_mutex);
    PROPAGATE_ERROR(sm.expectLocked());

    for (StreamList::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
    {
       if (it->m_consumer->isEGLStream(eglStream))
       {
            PROPAGATE_ERROR(it->m_consumer->setStreamAspectRatio(aspectRatio));
            return true;
        }
    }

    ORIGINATE_ERROR("Stream was not bound");

    return true;
}

bool Composer::onResize(uint32_t width, uint32_t height)
{
    m_windowWidth = width;
    m_windowHeight = height;
    m_windowAspectRatio = (float)width / (float)height;
    return true;
}

bool Composer::threadInitialize()
{
    // Initialize the GL context and make it current.
    PROPAGATE_ERROR(m_context.initialize(&Window::getInstance()));
    PROPAGATE_ERROR(m_context.makeCurrent());

    // Create the shader program.
    static const char vtxSrc[] =
        "#version 300 es\n"
        "#extension GL_ARB_explicit_uniform_location : require\n"
        "in layout(location = 0) vec2 vertex;\n"
        "out vec2 vTexCoord;\n"
        "layout(location = 0) uniform vec2 offset;\n"
        "layout(location = 1) uniform vec2 scale;\n"
        "void main() {\n"
        "  gl_Position = vec4((offset + vertex * scale) * 2.0 - 1.0, 0.0, 1.0);\n"
        "  vTexCoord = vec2(vertex.x, 1.0 - vertex.y);\n"
        "}\n";
    static const char frgSrc[] =
        "#version 300 es\n"
        "#extension GL_OES_EGL_image_external : require\n"
        "precision highp float;\n"
        "uniform samplerExternalOES texSampler;\n"
        "in vec2 vTexCoord;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "  fragColor = texture2D(texSampler, vTexCoord);\n"
        "}\n";
    PROPAGATE_ERROR(m_context.createProgram(vtxSrc, frgSrc, &m_program));

    glUseProgram(m_program);

    // Setup vertex state.
    static const GLfloat vertices[] = {
         0.0f, 0.0f,
         0.0f, 1.0f,
         1.0f, 0.0f,
         1.0f, 1.0f,
    };
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    if (!m_vbo)
        ORIGINATE_ERROR("Failed to create VBO");
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);

    // sync to the display refresh rate
    if (eglSwapInterval(m_display.get(), 1) != EGL_TRUE)
        ORIGINATE_ERROR("Failed to set the swap interval");

    return true;
}

bool Composer::renderStreams(uint32_t activeStreams)
{
    glViewport(0,0, m_windowWidth, m_windowHeight);

    glClear(GL_COLOR_BUFFER_BIT);

    const uint32_t cells = static_cast<uint32_t>(ceil(sqrt(activeStreams)));
    const float scaleX = 1.0f / cells;
    const float scaleY = 1.0f / cells;
    uint32_t offsetX = 0, offsetY = cells - 1;

    {
        ScopedMutex sm(m_mutex);
        PROPAGATE_ERROR(sm.expectLocked());

        for (StreamList::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
        {
            if (!it->m_active)
                continue;

            const EGLint streamState = it->m_consumer->getStreamState();
            if ((streamState == EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR) ||
                (streamState == EGL_STREAM_STATE_OLD_FRAME_AVAILABLE_KHR))
            {
                glBindTexture(GL_TEXTURE_EXTERNAL_OES, it->m_consumer->getStreamTextureID());

                // scale according to aspect ratios
                float sizeX = it->m_consumer->getStreamAspectRatio() / m_windowAspectRatio;
                float sizeY = 1.0f;

                if (sizeX > sizeY)
                {
                    sizeY /= sizeX;
                    sizeX = 1.0f;
                }
                else
                {
                    sizeX /= sizeY;
                    sizeY = 1.0f;
                }

                glUniform2f(0,
                    (offsetX - (sizeX - 1.0f) * 0.5f) * scaleX,
                    (offsetY - (sizeY - 1.0f) * 0.5f) * scaleY);
                glUniform2f(1, scaleX * sizeX, scaleY * sizeY);

                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }

            ++offsetX;
            if (offsetX == cells)
            {
                --offsetY;
                offsetX = 0;
            }
        }
    }

    PROPAGATE_ERROR(m_context.swapBuffers());

    PROPAGATE_ERROR(PerfTracker::getInstance().onEvent(GLOBAL_EVENT_DISPLAY));

    return true;
}

bool Composer::threadExecute()
{
    bool render = false;
    uint32_t activeStreams = 0;

    {
        ScopedMutex sm(m_mutex);
        PROPAGATE_ERROR(sm.expectLocked());

        if (m_streams.size() == 0)
            return true;

        // first iterate through the streams and check if there are streams which should be shutdown
        // also count the active streams
        for (StreamList::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
        {
            if (it->m_shutdown)
            {
                // shutdown the stream consumer if it had marked so
                PROPAGATE_ERROR_CONTINUE(it->m_consumer->shutdown());
                delete it->m_consumer;
                it = m_streams.erase(it);
                continue;
            }

            // do the acquire in any case even if the stream is not active (needed to get the
            // transition to connecting state)
            bool acquiredNewFrame = false;
            PROPAGATE_ERROR(it->m_consumer->acquire(&acquiredNewFrame));

            // check if the stream is active
            if (it->m_active)
            {
                ++activeStreams;
                // if a new frame is available we need to render
                if (acquiredNewFrame)
                    render = true;
            }
        }
    }

    if (render)
    {
        PROPAGATE_ERROR(renderStreams(activeStreams));
    }
    else
    {
        // wait some time and then check again if new frames are available
        usleep(1000);
    }

    return true;
}

bool Composer::threadShutdown()
{
    for (StreamList::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
    {
        PROPAGATE_ERROR_CONTINUE(it->m_consumer->shutdown());
        delete it->m_consumer;
    }
    m_streams.clear();

    if (m_program)
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    if (m_vbo)
    {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }

    PROPAGATE_ERROR(m_context.cleanup());

    return true;
}

}; // namespace ArgusSamples
