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

#include "PreviewConsumer.h"
#include "Error.h"

#include <string.h>
#include <math.h>
#include <Argus/Argus.h>

namespace ArgusSamples
{
#define PREVIEW_CONSUMER_PRINT(...) printf("PREVIEW CONSUMER: " __VA_ARGS__)

PreviewConsumerThread::PreviewConsumerThread(EGLDisplay display, EGLStreamKHR stream)
    : m_display(display)
    , m_streams(1, stream)
    , m_textures(1, 0)
    , m_program(0)
    , m_textureUniform(-1)
    , m_layout(LAYOUT_TILED)
    , m_syncStreams(true)
    , m_lineWidth(0)
{
    memset(m_lineColor, 0, sizeof(m_lineColor));
}

PreviewConsumerThread::PreviewConsumerThread(EGLDisplay display,
                                             const std::vector<EGLStreamKHR>& streams,
                                             RenderLayout layout,
                                             bool syncStreams)
    : m_display(display)
    , m_streams(streams)
    , m_textures(streams.size(), 0)
    , m_program(0)
    , m_textureUniform(-1)
    , m_layout(layout)
    , m_syncStreams(syncStreams)
    , m_lineWidth(0)
{
    memset(m_lineColor, 0, sizeof(m_lineColor));
}

PreviewConsumerThread::~PreviewConsumerThread()
{
}

bool PreviewConsumerThread::threadInitialize()
{

    Window &window = Window::getInstance();

    // Create the context and make it current.
    PREVIEW_CONSUMER_PRINT("Creating OpenGL context.\n");
    PROPAGATE_ERROR(m_context.initialize(&window));
    PROPAGATE_ERROR(m_context.makeCurrent());

    // Get window size.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    m_windowSize = Argus::Size2D<uint32_t>(viewport[2], viewport[3]);

    // Create the shader program to render a texture.
    static const GLfloat quadCoords[] = {1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    static const char vtxSrc[] =
        "#version 300 es\n"
        "in layout(location = 0) vec2 coord;\n"
        "out vec2 texCoord;\n"
        "void main() {\n"
        "  gl_Position = vec4((coord * 2.0) - 1.0, 0.0, 1.0);\n"
        // Note: Argus frames use a top-left origin and need to be inverted for GL texture use.
        "  texCoord = vec2(coord.x, 1.0 - coord.y);\n"
        "}\n";
    static const char frgSrc[] =
        "#version 300 es\n"
        "#extension GL_OES_EGL_image_external : require\n"
        "precision highp float;\n"
        "uniform samplerExternalOES texSampler;\n"
        "in vec2 texCoord;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "  fragColor = texture2D(texSampler, texCoord);\n"
        "}\n";
    PROPAGATE_ERROR(m_context.createProgram(vtxSrc, frgSrc, &m_program));
    glUseProgram(m_program);
    m_textureUniform = glGetUniformLocation(m_program, "texSampler");
    glUniform1i(m_textureUniform, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quadCoords);

    // For every stream, create an external texture and connect it to the stream as the consumer.
    PREVIEW_CONSUMER_PRINT("Connecting to EGLStream(s).\n");
    glGenTextures(m_textures.size(), &m_textures[0]);
    for (uint32_t i = 0; i < m_textures.size(); i++)
    {
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_textures[i]);
        if (!eglStreamConsumerGLTextureExternalKHR(m_display, m_streams[i]))
            ORIGINATE_ERROR("Unable to connect GL as consumer");

        // If the streams are synced, set the acquire timeouts to infinite so it will block
        // until new frames are available (so all streams will have the same frame acquired).
        // When not synced, streams will be acquired and rendered as soon as they're available.
        if (m_syncStreams)
        {
            eglStreamAttribKHR(m_display, m_streams[i],
                               EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, -1);
        }
    }
    PREVIEW_CONSUMER_PRINT("Connected to stream(s).\n");

    return true;
}

bool PreviewConsumerThread::threadExecute()
{
    EGLint state = EGL_STREAM_STATE_CONNECTING_KHR;
    Window &window = Window::getInstance();

    // Wait until the Argus producers are connected.
    PREVIEW_CONSUMER_PRINT("Waiting until producer(s) connect...\n");
    for (std::vector<EGLStreamKHR>::iterator s = m_streams.begin(); s != m_streams.end(); s++)
    {
        while (true)
        {
            if (!eglQueryStreamKHR(m_display, *s, EGL_STREAM_STATE_KHR, &state))
                ORIGINATE_ERROR("Failed to query stream state (possible producer failure).");
            if (state == EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR)
                break;
            window.pollEvents();
        }
    }
    PREVIEW_CONSUMER_PRINT("Producer(s) connected; continuing.\n");

    // Render as long as every stream is connected and the thread is still active.
    uint32_t frame = 0;
    bool done = false;
    while (!done && !m_doShutdown)
    {
        bool newFrameAvailable = false;
        for (uint32_t i = 0; i < m_streams.size(); i++)
        {
            if (!eglQueryStreamKHR(m_display, m_streams[i], EGL_STREAM_STATE_KHR, &state) ||
                state == EGL_STREAM_STATE_DISCONNECTED_KHR)
            {
                done = true;
                break;
            }
            else if (state == EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR)
            {
                newFrameAvailable = true;
                if (!eglStreamConsumerAcquireKHR(m_display, m_streams[i]))
                {
                    done = true;
                    break;
                }
            }
        }

        if (!done)
        {
            if (newFrameAvailable)
            {
                // Frame numbers will be different for each stream when they aren't synced,
                // so for now we don't bother printing the "acquired frame" message.
                /// @todo: Ideally we'd have an option to render a frame id text overlay on
                ///        top of each stream window.
                if (m_syncStreams)
                    PREVIEW_CONSUMER_PRINT("Acquired frame %d. Rendering.\n", ++frame);
                renderStreams();
                PROPAGATE_ERROR(m_context.swapBuffers());
            }
        }
        window.pollEvents();
    }
    PREVIEW_CONSUMER_PRINT("No more frames. Cleaning up.\n");

    PROPAGATE_ERROR(requestShutdown());

    return true;
}

void PreviewConsumerThread::renderStreams()
{
    glClearColor(m_lineColor[0], m_lineColor[1], m_lineColor[2], 1.0f);

    if (m_streams.size() == 1)
    {
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    else
    {
        if (m_layout == LAYOUT_HORIZONTAL)
        {
            int32_t streamWidth = m_windowSize.width() / m_streams.size();
            for (uint32_t i = 0; i < m_streams.size(); i++)
            {
                glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_textures[i]);
                glViewport(i * streamWidth, 0, streamWidth, m_windowSize.height());
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                if (m_lineWidth && i > 0)
                {
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(i * streamWidth - m_lineWidth / 2, 0,
                              m_lineWidth, m_windowSize.height());
                    glClear(GL_COLOR_BUFFER_BIT);
                    glDisable(GL_SCISSOR_TEST);
                }
            }
        }
        else if (m_layout == LAYOUT_VERTICAL)
        {
            int32_t streamHeight = m_windowSize.height() / m_streams.size();
            for (uint32_t i = 0; i < m_streams.size(); i++)
            {
                glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_textures[i]);
                glViewport(0, m_windowSize.height() - streamHeight * (i+1),
                           m_windowSize.width(), streamHeight);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                if (m_lineWidth && i > 0)
                {
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(0, m_windowSize.height() - streamHeight * i - m_lineWidth / 2,
                              m_windowSize.width(), m_lineWidth);
                    glClear(GL_COLOR_BUFFER_BIT);
                    glDisable(GL_SCISSOR_TEST);
                }
            }
        }
        else if (m_layout == LAYOUT_TILED)
        {
            uint32_t tileCount = ceil(sqrt(m_streams.size()));
            int32_t tileWidth = m_windowSize.width() / tileCount;
            int32_t tileHeight = m_windowSize.height() / tileCount;
            for (uint32_t i = 0; i < m_streams.size(); i++)
            {
                uint32_t row = (tileCount - 1) - (i / tileCount);
                uint32_t col = i % tileCount;
                glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_textures[i]);
                glViewport(col * tileWidth, row * tileHeight, tileWidth, tileHeight);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
            if (m_lineWidth)
            {
                glEnable(GL_SCISSOR_TEST);
                for (uint32_t i = 1; i < tileCount; i++)
                {
                    glScissor(0, m_windowSize.height() - tileHeight * i - m_lineWidth / 2,
                              m_windowSize.width(), m_lineWidth);
                    glClear(GL_COLOR_BUFFER_BIT);
                    glScissor(tileWidth * i - m_lineWidth / 2, 0,
                              m_lineWidth, m_windowSize.height());
                    glClear(GL_COLOR_BUFFER_BIT);
                }
                glDisable(GL_SCISSOR_TEST);
            }
        }
        else if (m_layout == LAYOUT_SPLIT_HORIZONTAL)
        {
            int32_t streamHeight = m_windowSize.height() / m_streams.size();
            glEnable(GL_SCISSOR_TEST);
            for (uint32_t i = 0; i < m_streams.size(); i++)
            {
                glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_textures[i]);
                glScissor(0, m_windowSize.height() - streamHeight * (i+1),
                          m_windowSize.width(), streamHeight);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                if (m_lineWidth && i > 0)
                {
                    glScissor(0, m_windowSize.height() - streamHeight * i - m_lineWidth / 2,
                              m_windowSize.width(), m_lineWidth);
                    glClear(GL_COLOR_BUFFER_BIT);
                }
            }
            glDisable(GL_SCISSOR_TEST);
        }
        else if (m_layout == LAYOUT_SPLIT_VERTICAL)
        {
            int32_t streamWidth = m_windowSize.width() / m_streams.size();
            glEnable(GL_SCISSOR_TEST);
            for (uint32_t i = 0; i < m_streams.size(); i++)
            {
                glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_textures[i]);
                glScissor(streamWidth * i, 0, streamWidth, m_windowSize.height());
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                if (m_lineWidth && i > 0)
                {
                    glScissor(streamWidth * i - m_lineWidth / 2, 0,
                              m_lineWidth, m_windowSize.height());
                    glClear(GL_COLOR_BUFFER_BIT);
                }
            }
            glDisable(GL_SCISSOR_TEST);
        }
    }

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
}

bool PreviewConsumerThread::threadShutdown()
{
    glDeleteProgram(m_program);
    glDeleteTextures(m_textures.size(), &m_textures[0]);
    m_context.cleanup();

    PREVIEW_CONSUMER_PRINT("Done.\n");

    return true;
}

void PreviewConsumerThread::setLineWidth(uint32_t width)
{
    m_lineWidth = width;
}

void PreviewConsumerThread::setLineColor(float r, float g, float b)
{
    m_lineColor[0] = r;
    m_lineColor[1] = g;
    m_lineColor[2] = b;
}

} // namespace ArgusSamples
