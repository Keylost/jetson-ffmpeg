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

#ifndef GLCONTEXT_H
#define GLCONTEXT_H

#define GL_GLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <stddef.h>

#include "Window.h"

namespace ArgusSamples
{

/**
 * Class that wraps an EGLContext and provides some utility functions
 * for commonly used EGL/GL features such as shader compilation.
 */
class GLContext
{
public:
    GLContext();
    ~GLContext();

    /**
     * Initialize, create surfaceless context on default display
     *
     * @param [in] sharingContext   Context to share with
     */
    bool initialize(const GLContext *sharingContext = NULL);

    /**
     * Initialize, create surfaceless context on 'display'
     *
     * @param [in] display
     * @param [in] sharingContext   Context to share with
     */
    bool initialize(EGLDisplay display, const GLContext *sharingContext = NULL);

    /**
     * Initialize, use surface from 'window'
     *
     * @param [in] window
     * @param [in] sharingContext   Context to share with
     */
    bool initialize(Window *window, const GLContext *sharingContext = NULL);

    /**
     * Cleanup
     */
    bool cleanup();

    /**
     * Make the context current.
     *
     * @param [in] surface  surface to make current to, if EGL_NO_SURFACE use the window surface
     */
    bool makeCurrent(EGLSurface surface = EGL_NO_SURFACE);

    /**
     * Unmake the context from being current.
     */
    bool unmakeCurrent();

    /**
     * Swap buffers.
     * For contexts attached to windows, swaps the default framebuffer.
     *
     * @param [in] surface  surface to swap on, if EGL_NO_SURFACE use the window surface
     */
    bool swapBuffers(EGLSurface surface = EGL_NO_SURFACE);

    /**
     * Create an EGLSurface as a producer of images to an EGLStream
     *
     * @param [out] surface     created surface
     * @param [in] eglStream    EGLStream
     * @param [in] width        width of the images that make up the stream
     * @param [in] height       height of the images that make up the stream
     */
    bool createEGLStreamProducerSurface(EGLSurface *surface, EGLStreamKHR eglStream,
        int32_t width, int32_t height);

    /**
     * Creates a new program from the specified vertex and fragment shader source.
     * @param[in] vtxSrc The vertex shader source.
     * @param[in] frgSrc The fragment shader source.
     * @param[out] program The output program id.
     */
    bool createProgram(const char* vtxSrc, const char* frgSrc, GLuint *program);

    /**
     * Set the text size.
     * @param[in] height normalized height relative to window.
     * @param[in] relativeWidth width scaling factor relative to normal size (>1 is wider).
     */
    void setTextSize(float height, float relativeWidth = 1.0f);

    /**
     * Set the text position.
     * Note that the text position will be automatically updated every time text is rendered
     * such that successive calls to renderText() will scroll in a standard left-to-right,
     * top-to-bottom manner without needing to explicitly set the position each time.
     * @param[in] x normalized position relative to left edge of window.
     * @param[in] x normalized position relative to bottom edge of window.
     */
    void setTextPosition(float x, float y);

    /**
     * Set the text color. All values are nomalized in [0, 1].
     * @param[in] r red color component.
     * @param[in] g green color component.
     * @param[in] b blue color component.
     * @param[in] a alpha color component.
     */
    void setTextColor(float r, float g, float b, float a = 1.0f);

    /**
     * Set the text background color. All values are normalized in [0, 1].
     * @param[in] r red color component.
     * @param[in] g green color component.
     * @param[in] b blue color component.
     * @param[in] a alpha color component.
     */
    void setTextBackground(float r, float g, float b, float a = 1.0f);

    /**
     * Renders text using the current text rendering state (see setText* functions).
     * Note that text rendering uses a 2D texture in texture unit 15, and vertex attrib arrays
     * 14 and 15, and this state is not reset before returning.
     * @param[in] text string to render.
     */
    bool renderText(const char* text);

    /**
     * Renders text using the current text rendering state (see setText* functions).
     * Note that text rendering uses a 2D texture in texture unit 15, and vertex attrib arrays
     * 14 and 15, and this state is not reset before returning.
     * @param[in] text string to render.
     */
    void renderTextf(const char* format, ...);

private:

    /**
     * Initialize
     */
    bool initializeInternal(const GLContext *sharingContext);

    /**
     * Creates and compiles a shader.
     * @param[in] type The shader type (eg. GL_VERTEX_SHADER).
     * @param[in] src The shader source.
     * @param[out] shader The output shader id.
     */
    bool createShader(GLenum type, const char* src, GLuint *shader);

    /**
     * Links a program object.
     * @param[in] program The program to link.
     */
    bool linkProgram(GLuint program);


    bool m_initialized;
    Window *m_window;
    EGLContext m_sharingContext;
    EGLConfig m_config;
    EGLDisplay m_display;
    EGLContext m_context;
    EGLSurface m_surface;

    /// Text rendering state.
    GLuint m_textProgram;
    GLuint m_textTexture;
    float m_textColor[4];
    float m_textBackground[4];
    float m_currentTextPosition[2];
    float m_userTextPosition[2];
    float m_textHeight;
    float m_textRelativeWidth;
};

}; // namespace ArgusSamples

#endif // GLCONTEXT_H
