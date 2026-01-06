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

#include <assert.h>
#include <stdarg.h>

#include "GLContext.h"
#include "EGLGlobal.h"
#include "Error.h"

#include "Courier16x24.h"

namespace ArgusSamples
{

GLContext::GLContext()
    : m_initialized(false)
    , m_window(NULL)
    , m_sharingContext(EGL_NO_CONTEXT)
    , m_config(0)
    , m_display(EGL_NO_DISPLAY)
    , m_context(EGL_NO_CONTEXT)
    , m_surface(EGL_NO_SURFACE)
    , m_textProgram(0)
    , m_textTexture(0)
    , m_textHeight(16.0f/480.0f) // Default to 16 pixels high for 480 pixel high windows.
    , m_textRelativeWidth(1.0f)
{
    m_textColor[0] = m_textColor[1] = m_textColor[2] = m_textColor[3] = 1.0f;
    m_textBackground[0] = m_textBackground[1] = m_textBackground[2] = m_textBackground[3] = 0.0f;
    m_currentTextPosition[0] = m_userTextPosition[0] = 0.0f;
    m_currentTextPosition[1] = m_userTextPosition[1] = 1.0f;
}

GLContext::~GLContext()
{
    cleanup();
}

bool GLContext::initialize(const GLContext *sharingContext)
{
    if (m_initialized)
        ORIGINATE_ERROR("Already initialized");

    m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    PROPAGATE_ERROR(initializeInternal(sharingContext));

    return true;
}

bool GLContext::initialize(EGLDisplay display, const GLContext *sharingContext)
{
    if (m_initialized)
        ORIGINATE_ERROR("Already initialized");

    m_display = display;
    PROPAGATE_ERROR(initializeInternal(sharingContext));

    return true;
}

bool GLContext::initialize(Window *window, const GLContext *sharingContext)
{
    if (m_initialized)
        ORIGINATE_ERROR("Already initialized");
    if (window == NULL)
        ORIGINATE_ERROR("'window' is NULL");

    m_window = window;
    m_display = eglGetDisplay(window->getEGLNativeDisplay());
    PROPAGATE_ERROR(initializeInternal(sharingContext));

    return true;
}

bool GLContext::initializeInternal(const GLContext *sharingContext)
{
    assert(!m_initialized);

    if (sharingContext)
    {
        if (m_window && sharingContext->m_window)
            ORIGINATE_ERROR("When sharing contexts at least one context must be surfaceless");

        // if no window is specified get the window and display from the sharing context
        if (!m_window)
        {
            m_window = sharingContext->m_window;
            m_display = eglGetDisplay(m_window->getEGLNativeDisplay());
        }

        m_sharingContext = sharingContext->m_context;
    }

    {
        static const EGLint configAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, 0x0,
            EGL_NONE
        };

        EGLint numConfigs = 0;
        EGLConfig configs[128];
        if (!eglChooseConfig(m_display, configAttribs, configs, 128, &numConfigs))
            ORIGINATE_ERROR("Could not query EGL configs (error 0x%04x)", eglGetError());

        if (sharingContext)
        {
            // Use the same config as the sharing context
            m_config = sharingContext->m_config;
        }
        else if (m_window)
        {
            // Choose the EGL config (32-bit color, 24-bit depth, no multisampling).
            for (EGLint config = 0; config < numConfigs; config++)
            {
                EGLint attrib;

                // Color bits.
                eglGetConfigAttrib(m_display, configs[config], EGL_BUFFER_SIZE, &attrib);
                if (attrib != 32)
                    continue;

                // Depth bits.
                eglGetConfigAttrib(m_display, configs[config], EGL_DEPTH_SIZE, &attrib);
                if (attrib != 24)
                    continue;

                // Samples.
                eglGetConfigAttrib(m_display, configs[config], EGL_SAMPLES, &attrib);
                if (attrib != 0)
                    continue;

                m_config = configs[config];
                break;
            }
            if (m_config == 0)
                ORIGINATE_ERROR("No suitable EGLConfig found");

            m_surface = m_window->getEGLSurface(m_config);
            if (m_surface == EGL_NO_SURFACE)
                ORIGINATE_ERROR("Could not get EGL surface");
        }
        else
        {
            // Get config. Since we're using a surfaceless context, first config will do.
            m_config = configs[0];
        }
    }

    // Create the context.
    {
        static const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
        };

        m_context = eglCreateContext(m_display, m_config, m_sharingContext, contextAttribs);
        if (m_context == EGL_NO_CONTEXT)
            ORIGINATE_ERROR("Could not create context (error 0x%04x)", eglGetError());
    }

    m_initialized = true;

    return true;
}

bool GLContext::cleanup()
{
    if (m_context != EGL_NO_CONTEXT)
    {
        if (!eglDestroyContext(m_display, m_context))
            REPORT_ERROR("Unable to destroy context (error 0x%04x)", eglGetError());
        m_context = EGL_NO_CONTEXT;
        if (!eglReleaseThread())
            REPORT_ERROR("Unable to release thread from EGL (error 0x%04x)", eglGetError());
    }

    m_initialized = false;

    return true;
}

bool GLContext::makeCurrent(EGLSurface surface)
{
    if (!m_initialized)
        ORIGINATE_ERROR("Context not initialized");

    if (!eglMakeCurrent(m_display, (surface == EGL_NO_SURFACE) ? m_surface : surface,
        (surface == EGL_NO_SURFACE) ? m_surface : surface, m_context))
    {
        ORIGINATE_ERROR("Unable to make context current (error 0x%04x)", eglGetError());
    }

    return true;
}

bool GLContext::unmakeCurrent()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Context not initialized");

    if (!eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
        ORIGINATE_ERROR("Unable to make context current (error 0x%04x)", eglGetError());

    if (eglReleaseThread() != EGL_TRUE)
        ORIGINATE_ERROR("eglReleaseThread failed (error 0x%04x)", eglGetError());

    return true;
}

bool GLContext::swapBuffers(EGLSurface surface)
{
    if (!m_initialized)
        ORIGINATE_ERROR("Context not initialized");

    EGLSurface swapSurface = (surface == EGL_NO_SURFACE) ? m_surface : surface;
    if (swapSurface == EGL_NO_SURFACE)
        ORIGINATE_ERROR("No surface provided and context does not have a window surface");

    if (!eglSwapBuffers(m_display, swapSurface))
        ORIGINATE_ERROR("Error calling eglSwapBuffers (error 0x%04x)", eglGetError());

    return true;
}

bool GLContext::createEGLStreamProducerSurface(EGLSurface *surface, EGLStreamKHR eglStream,
    int32_t width, int32_t height)
{
    if (!m_initialized)
        ORIGINATE_ERROR("Context is not initialized");
    if (surface == NULL)
        ORIGINATE_ERROR("'surface' is NULL");
    if (eglStream == EGL_NO_STREAM_KHR)
        ORIGINATE_ERROR("'eglStream' is EGL_NO_STREAM_KHR");

    const EGLint attribs[] =
    {
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_NONE
    };

    *surface = eglCreateStreamProducerSurfaceKHR(m_display, m_config, eglStream, attribs);
    if (*surface == EGL_NO_SURFACE)
        ORIGINATE_ERROR("Failed to create EGLSurface (error 0x%04x).", eglGetError());

    return true;
}

bool GLContext::createShader(GLenum type, const char* src, GLuint *shader)
{
    if (!m_initialized)
        ORIGINATE_ERROR("Context not initialized");
    if (!shader)
        ORIGINATE_ERROR("NULL shader pointer");

    // Create/compile the shader.
    GLuint shaderId = glCreateShader(type);
    if (!shaderId)
        ORIGINATE_ERROR("Unable to create shader");
    glShaderSource(shaderId, 1, &src, NULL);
    glCompileShader(shaderId);

    // Check compilation status and print any errors.
    GLint compileStatus;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compileStatus);
    if (compileStatus != GL_TRUE)
    {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shaderId, sizeof(infoLog), NULL, infoLog);
        fprintf(stderr, "%s shader compilation failed with log:\n%s\nShader source:\n%s\n",
                         (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment"), infoLog, src);
        glDeleteShader(shaderId);
        ORIGINATE_ERROR("Shader compile failed");
    }

    *shader = shaderId;
    return true;
}

bool GLContext::createProgram(const char* vtxSrc, const char* frgSrc, GLuint *program)
{
    if (!m_initialized)
        ORIGINATE_ERROR("Context not initialized");
    if (!program)
        ORIGINATE_ERROR( "NULL program pointer");

    // Create the program.
    GLuint programId = glCreateProgram();
    if (!programId)
        ORIGINATE_ERROR("Unable to create program");

    // Create and attach the shaders. We delete the shaders immediately after attaching
    // them to the program since the program will hold a reference to the shaders.
    GLuint vtxShader, frgShader;
    PROPAGATE_ERROR_FAIL(createShader(GL_VERTEX_SHADER, vtxSrc, &vtxShader));
    glAttachShader(programId, vtxShader);
    glDeleteShader(vtxShader);
    PROPAGATE_ERROR_FAIL(createShader(GL_FRAGMENT_SHADER, frgSrc, &frgShader));
    glAttachShader(programId, frgShader);
    glDeleteShader(frgShader);

    // Link the program.
    PROPAGATE_ERROR_FAIL(linkProgram(programId));

    *program = programId;
    return true;

fail:
    glDeleteProgram(programId);
    return false;
}

bool GLContext::linkProgram(GLuint program)
{
    if (!m_initialized)
        ORIGINATE_ERROR("Context not initialized");

    // Link the program and print any errors.
    glLinkProgram(program);
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE)
    {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, sizeof(infoLog), NULL, infoLog);
        fprintf(stderr, "Program linking failed with log:\n%s\n", infoLog);
        ORIGINATE_ERROR("Program link failed");
    }

    return true;
}

bool GLContext::renderText(const char* text)
{
    // Font texture constants.
    const unsigned char* fontPixels = courier16x24;
    const GLint fontTextureWidth = 256;
    const GLint fontTextureHeight = 144;
    const GLfloat fontWidth = 1.0f / 16.0f;
    const GLfloat fontHeight = 1.0f / 6.0f;
    const GLfloat fontAspect = 16.0f / 24.0f;
    const GLfloat fontSpacing = fontWidth / 32.0f;

    // Static vertex attrib arrays.
    static GLfloat verts[8];
    static GLfloat texCoords[8];

    EGLContext currentContext = eglGetCurrentContext();
    if (currentContext != m_context)
        ORIGINATE_ERROR("Context not current");

    GLint currentProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);

    // Initialize the shader and texture on first use.
    if (!m_textProgram)
    {
        static const char* vtxSrc =
            "#version 300 es\n"
            "in layout(location = 14) vec4 vertex;\n"
            "in layout(location = 15) vec2 texCoord;\n"
            "out vec2 vTexCoord;\n"
            "void main() {\n"
            "  gl_Position = (2.0 * vertex) - 1.0;\n"
            "  vTexCoord = texCoord;\n"
            "}\n";
        static const char frgSrc[] =
            "#version 300 es\n"
            "precision mediump float;\n"
            "uniform sampler2D texSampler;\n"
            "uniform vec4 color;\n"
            "uniform vec4 background;\n"
            "in vec2 vTexCoord;\n"
            "out vec4 fragColor;\n"
            "void main() {\n"
            "  float textAlpha = texture2D(texSampler, vTexCoord).r * color.a;\n"
            "  fragColor.a = textAlpha + background.a;\n"
            "  fragColor.rgb = color.rgb * textAlpha + \n"
            "                  background.rgb * (1.0 - textAlpha) * background.a;\n"
            "}\n";
        PROPAGATE_ERROR(createProgram(vtxSrc, frgSrc, &m_textProgram));
        glUseProgram(m_textProgram);
        glUniform1i(glGetUniformLocation(m_textProgram, "texSampler"), 15);

        glVertexAttribPointer(14, 2, GL_FLOAT, GL_FALSE, 0, verts);
        glVertexAttribPointer(15, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
        glEnableVertexAttribArray(14);
        glEnableVertexAttribArray(15);

        GLint activeTexture;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glActiveTexture(GL_TEXTURE15);
        glGenTextures(1, &m_textTexture);
        glBindTexture(GL_TEXTURE_2D, m_textTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, fontTextureWidth, fontTextureHeight, 0,
                     GL_RED, GL_UNSIGNED_BYTE, fontPixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glActiveTexture(activeTexture);
    }

    glUseProgram(m_textProgram);

    // Set the text color.
    glUniform4fv(glGetUniformLocation(m_textProgram, "color"), 1, m_textColor);
    glUniform4fv(glGetUniformLocation(m_textProgram, "background"), 1, m_textBackground);

    // Enable alpha blending.
    GLint blendEnable;
    glGetIntegerv(GL_BLEND, &blendEnable);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render the text.
    GLfloat &x = m_currentTextPosition[0];
    GLfloat &y = m_currentTextPosition[1];
    const GLfloat textHeight = m_textHeight;
    const GLfloat textWidth  = m_textHeight * m_textRelativeWidth * fontAspect;
    while (*text)
    {
        // Set the vertex positions.
        verts[0] = x + textWidth;
        verts[1] = y - textHeight;
        verts[2] = x + textWidth;
        verts[3] = y;
        verts[4] = x;
        verts[5] = y - textHeight;
        verts[6] = x;
        verts[7] = y;

        // Set the texture coordinates.
        int col = *text % 16;
        int row = *text / 16 - 2;
        texCoords[0] = fontWidth  * (col + 1) - fontSpacing;
        texCoords[1] = fontHeight * (row + 1);
        texCoords[2] = fontWidth  * (col + 1) - fontSpacing;
        texCoords[3] = fontHeight * row;
        texCoords[4] = fontWidth  * col + fontSpacing;
        texCoords[5] = fontHeight * (row + 1);
        texCoords[6] = fontWidth  * col + fontSpacing;
        texCoords[7] = fontHeight * row;

        // Render the character.
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Advance character and/or line.
        x += textWidth;
        text++;
        if (*text == '\n')
        {
            x = m_userTextPosition[0];
            y -= textHeight;
            text++;
        }
    }

    // Reset state.
    glUseProgram(currentProgram);
    if (!blendEnable)
        glDisable(GL_BLEND);

    return true;
}

void GLContext::renderTextf(const char* format, ...)
{
    va_list argList;

    va_start(argList, format);
    char ps[1024];
    vsnprintf(ps,sizeof(ps), format, argList);
    renderText(ps);
    va_end(argList);
}

void GLContext::setTextSize(float height, float relativeWidth)
{
    m_textHeight = height;
    m_textRelativeWidth = relativeWidth;
}

void GLContext::setTextPosition(float x, float y)
{
    m_currentTextPosition[0] = x;
    m_currentTextPosition[1] = y;
    m_userTextPosition[0] = x;
    m_userTextPosition[1] = y;
}

void GLContext::setTextColor(float r, float g, float b, float a)
{
    m_textColor[0] = r;
    m_textColor[1] = g;
    m_textColor[2] = b;
    m_textColor[3] = a;
}

void GLContext::setTextBackground(float r, float g, float b, float a)
{
    m_textBackground[0] = r;
    m_textBackground[1] = g;
    m_textBackground[2] = b;
    m_textBackground[3] = a;
}

}; // namespace ArgusSamples
