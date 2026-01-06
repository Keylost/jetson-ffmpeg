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

/**
 * @file
 * <b>NVIDIA Multimedia API: EGL Renderer API</b>
 *
 * @b Description: This file declares the NvEgl Renderer API.
 */
#ifndef __NV_EGL_RENDERER_H__
#define __NV_EGL_RENDERER_H__

#include "NvElement.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <X11/Xlib.h>

/**
 * @defgroup l4t_mm_nveglrenderer_group Rendering API
 *
 * The \c %NvEglRenderer API provides EGL and Open GL ES 2.0 rendering
 * functionality.
 *
 * @ingroup aa_framework_api_group
 * @{
 */

/**
 *
 * @c %NvEglRenderer is a helper class for rendering using EGL and OpenGL
 * ES 2.0. The renderer requires the file descriptor (FD) of a buffer
 * as an input. The rendering rate, in frames per second (fps), is
 * configurable.
 *
 * The renderer creates an X Window of its own. The width, height,
 * horizontal offset, and vertical offset of the window are
 * configurable.
 *
 * All EGL calls must be made through one thread only. This class
 * internally creates a thread which performs all EGL/GL
 * initializations, gets @c EGLImage objects from FD, renders the @c
 * EGLImage objects, and then deinitializes all the EGL/GL structures.
 *
 */
class NvEglRenderer:public NvElement
{
public:
    /**
     * Creates a new EGL-based renderer named @a name.
     *
     * This method creates a new X window for rendering, of size @a
     * width and @a height, that is offset by @a x_offset and @a
     * y_offset. If @a width or @a height is zero, a full screen
     * window is created with @a x_offset and @a y_offset set to zero.
     *
     * It internally initializes EGL, creates an @c eglContext, an
     * @c eglSurface, a GL texture, and shaders for rendering.
     *
     *
     * @param[in] name Specifies a pointer to a unique name to identity the
     *                 element instance.
     * @param[in] width Specifies the width of the window in pixels.
     * @param[in] height Specifies the height of the window in pixels.
     * @param[in] x_offset Specifies the horizontal offset of the window in pixels.
     * @param[in] y_offset Specifies the vertical offset of the window in pixels.
     * @return A reference to the newly created renderer object, otherwise @c NULL in
     *          case of failure during initialization.
     */
    static NvEglRenderer *createEglRenderer(const char *name, uint32_t width,
                                          uint32_t height, uint32_t x_offset,
                                          uint32_t y_offset);
     ~NvEglRenderer();

    /**
     * Renders a buffer.
     *
     * This method waits until the rendering time of the next buffer,
     * caluclated from the rendering time of the last buffer and the
     * render rate in frames per second (fps). This is a blocking
     * call.
     *
     * @param[in] fd Specifies the file descriptor (FD) of the exported buffer
     *               to render.
     * @return 0 for success, -1 otherwise.
     */
    int render(int fd);

    /**
     * Sets the rendering rate in frames per second (fps).
     *
     * @warning An @a fps of zero is not allowed.
     *
     * @param[in] fps Specifies the render rate in fps.
     * @return 0 for success, -1 otherwise.
     */
    int setFPS(float fps);

    /**
     * Gets underlying EGLDisplay.
     *
     * @return EGLDisplay handle
     */
    EGLDisplay getEGLDisplay() { return egl_display; }

    /**
     * Gets the display resolution.
     *
     *
     * @param[out] width A pointer to the full screen width, in pixels.
     * @param[out] height A pointer to the full screen height, in pixels.
     * @return 0 for success, -1 otherwise.
     */
    static int getDisplayResolution(uint32_t &width, uint32_t &height);

    /**
     * Sets the overlay string.
     *
     *
     * @param[in] str A pointer to the overlay text.
     * @param[in] x Horizontal offset, in pixels.
     * @param[in] y Vertical offset, in pixels.
     * @return 0 for success, -1 otherwise.
     */
    int setOverlayText(char *str, uint32_t x, uint32_t y);

private:
    Display * x_display;    /**< Connection to the X server created using
                                  XOpenDisplay(). */
    Window x_window;        /**< Holds the window to be used for rendering created using
                                  XCreateWindow(). */

    EGLDisplay egl_display;     /**< Holds the EGL Display connection. */
    EGLContext egl_context;     /**< Holds the EGL rendering context. */
    EGLSurface egl_surface;     /**< Holds the EGL Window render surface. */
    EGLConfig egl_config;       /**< Holds the EGL frame buffer configuration to be used
                                     for rendering. */

    uint32_t texture_id;        /**< Holds the GL Texture ID used for rendering. */
    GC gc;                      /**< Graphic Context */
    XFontStruct *fontinfo;      /**< Brush's font info */
    char overlay_str[512];       /**< Overlay's text */

    /**
     * Creates a GL texture used for rendering.
     *
     * @return 0 for success, -1 otherwise.
     */
    int create_texture();
    /**
     * Initializes shaders with shader programs required for drawing a
     * buffer.
     *
     * @return 0 for success, -1 otherwise.
     */
    int InitializeShaders();
    /**
     * Creates, compiles and attaches a shader to the @a program.
     *
     * @param[in] program Specifies the GL Program ID.
     * @param[in] type Specifies the type of the vertex shader. Must be either
                       @c GL_VERTEX_SHADER or @c GL_FRAGMENT_SHADER.
     * @param[in] source Specifies the source code of the shader in form of a string.
     * @param[in] size Specifies the character length of @a source.
     */
    void CreateShader(GLuint program, GLenum type, const char *source,
                      int size);

    struct timespec last_render_time;   /**< Rendering time for the last buffer. */

    int render_fd;      /**< File descriptor (FD) of the next buffer to
                             render. */
    bool stop_thread;   /**< Boolean variable used to signal rendering thread
                             to stop. */
    pthread_t render_thread;        /**< The pthread id of the rendering thread. */
    pthread_mutex_t render_lock;    /**< Used for synchronization. */
    pthread_cond_t render_cond;     /**< Used for synchronization. */
    uint32_t overlay_str_x_offset;  /**< Overlay text's position in horizontal direction. */
    uint32_t overlay_str_y_offset;  /**< Overlay text's position in vertical direction. */
    float fps;                      /**< The render rate in frames per second. */
    uint64_t render_time_sec;       /**< Seconds component of the time for which a
                                         frame should be displayed. */
    uint64_t render_time_nsec;      /**< Nanoseconds component of the time for which
                                         a frame should be displayed. */

    /**
     * Constructor called by the wrapper createEglRenderer.
     */
    NvEglRenderer(const char *name, uint32_t width, uint32_t height,
                  uint32_t x_offset, uint32_t y_offset);
    /**
     * Gets the pointers to the required EGL methods.
     */
    static int initEgl();
    /**
     * Method executed by the renderThread.
     *
     * This method continues to execute infinitely until signalled to
     * stop with the @c stop_thread variable. This method contains a
     * while loop that calls NvEglRenderer::renderInternal().
     */
    static void * renderThread(void *arg);
    /**
     * This method contains the actual logic of rendering a buffer
     * and waiting until the buffer render time.
     */
    int renderInternal();

    /**
     * These EGL function pointers are required by the renderer.
     */
    static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
    static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
    static PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
    static PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
    static PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR;
    static PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR;
    static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

    static const NvElementProfiler::ProfilerField valid_fields =
            NvElementProfiler::PROFILER_FIELD_TOTAL_UNITS |
            NvElementProfiler::PROFILER_FIELD_FPS |
            NvElementProfiler::PROFILER_FIELD_LATE_UNITS;
};
/** @} */
#endif
