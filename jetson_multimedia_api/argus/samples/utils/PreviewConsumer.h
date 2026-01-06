/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
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

#ifndef PREVIEWCONSUMER_H
#define PREVIEWCONSUMER_H

#include <Argus/Argus.h>
#include "EGLGlobal.h"
#include "GLContext.h"
#include "Thread.h"
#include "Window.h"

namespace ArgusSamples
{

using namespace Argus;

/*******************************************************************************
 * Preview consumer thread:
 *   Uses an on-screen window and and OpenGL EGLStream consumer to render a live
 *   preview of an OutputStream to the display.
 ******************************************************************************/
class PreviewConsumerThread : public Thread
{
public:

    enum RenderLayout {
        /// Streams divide the window horizontally, first on the left, last on the right.
        /// Expected window size is (height, width * streams.size())
        LAYOUT_HORIZONTAL,

        /// Streams divide the window vertically, first at the top, last at the bottom.
        /// Expected window size is (height * streams.size(), width)
        LAYOUT_VERTICAL,

        /// Streams are tiled evenly (eg. 2x2, 3x3, etc.) to maintain window aspect ratio.
        LAYOUT_TILED,

        /// Streams are divided horizontally to render to a single region.
        LAYOUT_SPLIT_HORIZONTAL,

        /// Streams are divided vertically to render to a single region.
        LAYOUT_SPLIT_VERTICAL,
    };

    explicit PreviewConsumerThread(EGLDisplay display, EGLStreamKHR stream);
    explicit PreviewConsumerThread(EGLDisplay display, const std::vector<EGLStreamKHR>& streams,
                                   RenderLayout layout = LAYOUT_TILED,
                                   bool syncStreams = false);
    ~PreviewConsumerThread();

    /**
     * Sets the width of the line to render between streams.
     */
    void setLineWidth(uint32_t width);

    /**
     * Sets the line color.
     */
    void setLineColor(float r, float g, float b);

private:
    /** @name Thread methods */
    /**@{*/
    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();
    /**@}*/

    void renderStreams();

    EGLDisplay m_display;
    GLContext m_context;
    std::vector<EGLStreamKHR> m_streams;
    std::vector<GLuint> m_textures;
    GLuint m_program;
    GLint m_textureUniform;
    Size2D<uint32_t> m_windowSize;
    RenderLayout m_layout;
    bool m_syncStreams;
    uint32_t m_lineWidth;
    float m_lineColor[3];
};

} // namespace ArgusSamples

#endif // PREVIEWCONSUMER_H
