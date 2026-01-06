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

#include "NvEglRenderer.h"
#include "NvLogging.h"
#include "nvbuf_utils.h"

#include <cstring>
#include <sys/time.h>

#define CAT_NAME "EglRenderer"

#define ERROR_GOTO_FAIL(val, string) \
    do { \
        if (val) {\
            CAT_DEBUG_MSG(string); \
            goto fail; \
        } \
    } while (0)

PFNEGLCREATEIMAGEKHRPROC                NvEglRenderer::eglCreateImageKHR;
PFNEGLDESTROYIMAGEKHRPROC               NvEglRenderer::eglDestroyImageKHR;
PFNEGLCREATESYNCKHRPROC                 NvEglRenderer::eglCreateSyncKHR;
PFNEGLDESTROYSYNCKHRPROC                NvEglRenderer::eglDestroySyncKHR;
PFNEGLCLIENTWAITSYNCKHRPROC             NvEglRenderer::eglClientWaitSyncKHR;
PFNEGLGETSYNCATTRIBKHRPROC              NvEglRenderer::eglGetSyncAttribKHR;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC     NvEglRenderer::glEGLImageTargetTexture2DOES;

using namespace std;

NvEglRenderer::NvEglRenderer(const char *name, uint32_t width, uint32_t height,
        uint32_t x_offset, uint32_t y_offset)
        :NvElement(name, valid_fields)
{
    int depth;
    int screen_num;
    XSetWindowAttributes window_attributes;
    x_window = 0;
    x_display = NULL;

    texture_id = 0;
    gc = NULL;
    fontinfo = NULL;

    egl_surface = EGL_NO_SURFACE;
    egl_context = EGL_NO_CONTEXT;
    egl_display = EGL_NO_DISPLAY;
    egl_config = NULL;

    memset(&last_render_time, 0, sizeof(last_render_time));
    stop_thread = false;
    render_thread = 0;
    render_fd = 0;

    memset(overlay_str, 0, sizeof(overlay_str));
    overlay_str_x_offset = 0;
    overlay_str_y_offset = 0;

    pthread_mutex_init(&render_lock, NULL);
    pthread_cond_init(&render_cond, NULL);

    setFPS(30);

    if (initEgl() < 0)
    {
        COMP_ERROR_MSG("Error getting EGL function addresses");
        goto error;
    }

    x_display = XOpenDisplay(NULL);
    if (NULL == x_display)
    {
        COMP_ERROR_MSG("Error in opening display");
        goto error;
    }

    screen_num = DefaultScreen(x_display);
    if (!width || !height)
    {
        width = DisplayWidth(x_display, screen_num);
        height = DisplayHeight(x_display, screen_num);
        x_offset = 0;
        y_offset = 0;
    }
    COMP_INFO_MSG("Setting Screen width " << width << " height " << height);

    COMP_DEBUG_MSG("Display opened successfully " << (size_t) x_display);

    depth = DefaultDepth(x_display, DefaultScreen(x_display));

    window_attributes.background_pixel =
        BlackPixel(x_display, DefaultScreen(x_display));

    window_attributes.override_redirect = 1;

    x_window = XCreateWindow(x_display,
                             DefaultRootWindow(x_display), x_offset,
                             y_offset, width, height,
                             0,
                             depth, CopyFromParent,
                             CopyFromParent,
                             (CWBackPixel | CWOverrideRedirect),
                             &window_attributes);

    XSelectInput(x_display, (int32_t) x_window, ExposureMask);
    XMapWindow(x_display, (int32_t) x_window);
    gc = XCreateGC(x_display, x_window, 0, NULL);

    XSetForeground(x_display, gc,
                WhitePixel(x_display, DefaultScreen(x_display)) );
    fontinfo = XLoadQueryFont(x_display, "9x15bold");

    pthread_mutex_lock(&render_lock);
    pthread_create(&render_thread, NULL, renderThread, this);
    pthread_setname_np(render_thread, "EglRenderer");
    pthread_cond_wait(&render_cond, &render_lock);
    pthread_mutex_unlock(&render_lock);

    if(isInError())
    {
        pthread_join(render_thread, NULL);
        goto error;
    }

    COMP_DEBUG_MSG("Renderer started successfully")
    return;

error:
    COMP_ERROR_MSG("Got ERROR closing display");
    is_in_error = 1;
}

int
NvEglRenderer::getDisplayResolution(uint32_t &width, uint32_t &height)
{
    int screen_num;
    Display * x_display = XOpenDisplay(NULL);
    if (NULL == x_display)
    {
        return  -1;
    }

    screen_num = DefaultScreen(x_display);
    width = DisplayWidth(x_display, screen_num);
    height = DisplayHeight(x_display, screen_num);

    XCloseDisplay(x_display);
    x_display = NULL;

    return 0;
}

void *
NvEglRenderer::renderThread(void *arg)
{
    EGLBoolean egl_status;
    NvEglRenderer *renderer = (NvEglRenderer *) arg;
    const char *comp_name = renderer->comp_name;

    static EGLint rgba8888[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE,
    };
    int num_configs = 0;
    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    renderer->egl_display = eglGetDisplay(renderer->x_display);
    if (EGL_NO_DISPLAY == renderer->egl_display)
    {
        COMP_ERROR_MSG("Unable to get egl display");
        goto error;
    }
    COMP_DEBUG_MSG("Egl Got display " << (size_t) renderer->egl_display);

    egl_status = eglInitialize(renderer->egl_display, 0, 0);
    if (!egl_status)
    {
        COMP_ERROR_MSG("Unable to initialize egl library");
        goto error;
    }

    egl_status =
        eglChooseConfig(renderer->egl_display, rgba8888,
                            &renderer->egl_config, 1, &num_configs);
    if (!egl_status)
    {
        COMP_ERROR_MSG("Error at eglChooseConfig");
        goto error;
    }
    COMP_DEBUG_MSG("Got numconfigs as " << num_configs);

    renderer->egl_context =
        eglCreateContext(renderer->egl_display, renderer->egl_config,
                            EGL_NO_CONTEXT, context_attribs);
    if (eglGetError() != EGL_SUCCESS)
    {
        COMP_ERROR_MSG("Got Error in eglCreateContext " << eglGetError());
        goto error;
    }
    renderer->egl_surface =
        eglCreateWindowSurface(renderer->egl_display, renderer->egl_config,
                (EGLNativeWindowType) renderer->x_window, NULL);
    if (renderer->egl_surface == EGL_NO_SURFACE)
    {
        COMP_ERROR_MSG("Error in creating egl surface " << eglGetError());
        goto error;
    }

    eglMakeCurrent(renderer->egl_display, renderer->egl_surface,
                    renderer->egl_surface, renderer->egl_context);
    if (eglGetError() != EGL_SUCCESS)
    {
        COMP_ERROR_MSG("Error in eglMakeCurrent " << eglGetError());
        goto error;
    }

    if (renderer->InitializeShaders() < 0)
    {
        COMP_ERROR_MSG("Error while initializing shaders");
        goto error;
    }

    renderer->create_texture();

    pthread_mutex_lock(&renderer->render_lock);
    pthread_cond_broadcast(&renderer->render_cond);
    COMP_DEBUG_MSG("Starting render thread");

    while (!renderer->isInError() && !renderer->stop_thread)
    {
        pthread_cond_wait(&renderer->render_cond, &renderer->render_lock);
        pthread_mutex_unlock(&renderer->render_lock);

        if(renderer->stop_thread)
        {
            pthread_mutex_lock(&renderer->render_lock);
            break;
        }

        renderer->renderInternal();
        COMP_DEBUG_MSG("Rendered fd=" << renderer->render_fd);

        pthread_mutex_lock(&renderer->render_lock);
        pthread_cond_broadcast(&renderer->render_cond);
    }
    pthread_mutex_unlock(&renderer->render_lock);
    COMP_DEBUG_MSG("Stopped render thread");

finish:
    if (renderer->texture_id)
    {
        glDeleteTextures(1, &renderer->texture_id);
    }

    if (renderer->egl_display != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(renderer->egl_display, EGL_NO_SURFACE,
                EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    if (renderer->egl_surface != EGL_NO_SURFACE)
    {
        egl_status = eglDestroySurface(renderer->egl_display,
                renderer->egl_surface);
        if (egl_status == EGL_FALSE)
        {
            COMP_ERROR_MSG("EGL surface destruction failed");
        }
    }

    if (renderer->egl_context != EGL_NO_CONTEXT)
    {
        egl_status = eglDestroyContext(renderer->egl_display,
                renderer->egl_context);
        if (egl_status == EGL_FALSE)
        {
            COMP_ERROR_MSG("EGL context destruction failed");
        }
    }

    if (renderer->egl_display != EGL_NO_DISPLAY)
    {
        eglReleaseThread();
        eglTerminate(renderer->egl_display);
    }

    pthread_mutex_lock(&renderer->render_lock);
    pthread_cond_broadcast(&renderer->render_cond);
    pthread_mutex_unlock(&renderer->render_lock);
    return NULL;

error:
    renderer->is_in_error = 1;
    goto finish;
}

NvEglRenderer::~NvEglRenderer()
{
    stop_thread = true;

    pthread_mutex_lock(&render_lock);
    pthread_cond_broadcast(&render_cond);
    pthread_mutex_unlock(&render_lock);

    pthread_join(render_thread, NULL);

    pthread_mutex_destroy(&render_lock);
    pthread_cond_destroy(&render_cond);

    if (fontinfo)
    {
        XFreeFont(x_display, fontinfo);
    }

    if (gc)
    {
        XFreeGC(x_display, gc);
    }

    if (x_window)
    {
        XUnmapWindow(x_display, (int32_t) x_window);
        XFlush(x_display);
        XDestroyWindow(x_display, (int32_t) x_window);
    }
    if (x_display)
    {
        XCloseDisplay(x_display);
    }
}

int
NvEglRenderer::render(int fd)
{
    this->render_fd = fd;
    pthread_mutex_lock(&render_lock);
    pthread_cond_broadcast(&render_cond);
    COMP_DEBUG_MSG("Rendering fd=" << fd);
    pthread_cond_wait(&render_cond, &render_lock);
    pthread_mutex_unlock(&render_lock);
    return 0;
}

int
NvEglRenderer::renderInternal()
{
    EGLImageKHR hEglImage;
    bool frame_is_late = false;

    EGLSyncKHR egl_sync;
    int iErr;
    hEglImage = NvEGLImageFromFd(egl_display, render_fd);
    if (!hEglImage)
    {
        COMP_ERROR_MSG("Could not get EglImage from fd. Not rendering");
        return -1;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, hEglImage);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    iErr = glGetError();
    if (iErr != GL_NO_ERROR)
    {
        COMP_ERROR_MSG("glDrawArrays arrays failed:" << iErr);
        return -1;
    }
    egl_sync = eglCreateSyncKHR(egl_display, EGL_SYNC_FENCE_KHR, NULL);
    if (egl_sync == EGL_NO_SYNC_KHR)
    {
        COMP_ERROR_MSG("eglCreateSyncKHR() failed");
        return -1;
    }
    if (last_render_time.tv_sec != 0)
    {
        pthread_mutex_lock(&render_lock);
        last_render_time.tv_sec += render_time_sec;
        last_render_time.tv_nsec += render_time_nsec;
        last_render_time.tv_sec += last_render_time.tv_nsec / 1000000000UL;
        last_render_time.tv_nsec %= 1000000000UL;

        if (isProfilingEnabled())
        {
            struct timeval cur_time;
            gettimeofday(&cur_time, NULL);
            if ((cur_time.tv_sec * 1000000.0 + cur_time.tv_usec) >
                    (last_render_time.tv_sec * 1000000.0 +
                     last_render_time.tv_nsec / 1000.0))
            {
                frame_is_late = true;
            }
        }

        pthread_cond_timedwait(&render_cond, &render_lock,
                &last_render_time);

        pthread_mutex_unlock(&render_lock);
    }
    else
    {
        struct timeval now;

        gettimeofday(&now, NULL);
        last_render_time.tv_sec = now.tv_sec;
        last_render_time.tv_nsec = now.tv_usec * 1000L;
    }
    eglSwapBuffers(egl_display, egl_surface);
    if (eglGetError() != EGL_SUCCESS)
    {
        COMP_ERROR_MSG("Got Error in eglSwapBuffers " << eglGetError());
        return -1;
    }
    if (eglClientWaitSyncKHR (egl_display, egl_sync,
                EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR) == EGL_FALSE)
    {
        COMP_ERROR_MSG("eglClientWaitSyncKHR failed!");
    }

    if (eglDestroySyncKHR(egl_display, egl_sync) != EGL_TRUE)
    {
        COMP_ERROR_MSG("eglDestroySyncKHR failed!");
    }
    NvDestroyEGLImage(egl_display, hEglImage);

    if (strlen(overlay_str) != 0)
    {
        XSetForeground(x_display, gc,
                        BlackPixel(x_display, DefaultScreen(x_display)));
        XSetFont(x_display, gc, fontinfo->fid);
        XDrawString(x_display, x_window, gc, overlay_str_x_offset,
                    overlay_str_y_offset, overlay_str, strlen(overlay_str));
    }

    profiler.finishProcessing(0, frame_is_late);

    return 0;
}

int
NvEglRenderer::setOverlayText(char *str, uint32_t x, uint32_t y)
{
    strncpy(overlay_str, str, sizeof(overlay_str));
    overlay_str[sizeof(overlay_str) - 1] = '\0';

    overlay_str_x_offset = x;
    overlay_str_y_offset = y;

    return 0;
}

int
NvEglRenderer::setFPS(float fps)
{
    uint64_t render_time_usec;

    if (fps == 0)
    {
        COMP_WARN_MSG("Fps 0 is not allowed. Not changing fps");
        return -1;
    }
    pthread_mutex_lock(&render_lock);
    this->fps = fps;

    render_time_usec = 1000000L / fps;
    render_time_sec = render_time_usec / 1000000;
    render_time_nsec = (render_time_usec % 1000000) * 1000L;
    pthread_mutex_unlock(&render_lock);
    return 0;
}

NvEglRenderer *
NvEglRenderer::createEglRenderer(const char *name, uint32_t width,
                               uint32_t height, uint32_t x_offset,
                               uint32_t y_offset)
{
    NvEglRenderer* renderer = new NvEglRenderer(name, width, height,
                                    x_offset, y_offset);
    if (renderer->isInError())
    {
        delete renderer;
        return NULL;
    }
    return renderer;
}

int
NvEglRenderer::initEgl()
{
    eglCreateImageKHR =
        (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
    ERROR_GOTO_FAIL(!eglCreateImageKHR,
                    "ERROR getting proc addr of eglCreateImageKHR\n");

    eglDestroyImageKHR =
        (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");
    ERROR_GOTO_FAIL(!eglDestroyImageKHR,
                    "ERROR getting proc addr of eglDestroyImageKHR\n");

    eglCreateSyncKHR =
        (PFNEGLCREATESYNCKHRPROC) eglGetProcAddress("eglCreateSyncKHR");
    ERROR_GOTO_FAIL(!eglCreateSyncKHR,
                    "ERROR getting proc addr of eglCreateSyncKHR\n");

    eglDestroySyncKHR =
        (PFNEGLDESTROYSYNCKHRPROC) eglGetProcAddress("eglDestroySyncKHR");
    ERROR_GOTO_FAIL(!eglDestroySyncKHR,
                    "ERROR getting proc addr of eglDestroySyncKHR\n");

    eglClientWaitSyncKHR =
        (PFNEGLCLIENTWAITSYNCKHRPROC) eglGetProcAddress("eglClientWaitSyncKHR");
    ERROR_GOTO_FAIL(!eglClientWaitSyncKHR,
                    "ERROR getting proc addr of eglClientWaitSyncKHR\n");

    eglGetSyncAttribKHR =
        (PFNEGLGETSYNCATTRIBKHRPROC) eglGetProcAddress("eglGetSyncAttribKHR");
    ERROR_GOTO_FAIL(!eglGetSyncAttribKHR,
                    "ERROR getting proc addr of eglGetSyncAttribKHR\n");

    glEGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
        eglGetProcAddress("glEGLImageTargetTexture2DOES");
    ERROR_GOTO_FAIL(!glEGLImageTargetTexture2DOES,
                    "ERROR getting proc addr of glEGLImageTargetTexture2DOES\n");

    return 0;

fail:
    return -1;
}

void
NvEglRenderer::CreateShader(GLuint program, GLenum type, const char *source,
        int size)
{

    char log[4096];
    int result = GL_FALSE;

    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &source, &size);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
    if (!result)
    {
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        COMP_DEBUG_MSG("Got Fatal Log as " << log);
    }
    glAttachShader(program, shader);

    if (glGetError() != GL_NO_ERROR)
    {
        COMP_ERROR_MSG("Got gl error as " << glGetError());
    }
}

int
NvEglRenderer::InitializeShaders(void)
{
    GLuint program;
    int result = GL_FALSE;
    char log[4096];
    uint32_t pos_location = 0;

    // pos_x, pos_y, uv_u, uv_v
    float vertexTexBuf[24] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
    };

    static const char kVertexShader[] = "varying vec2 interp_tc;\n"
        "attribute vec4 in_pos;\n"
        "void main() { \n"
        "interp_tc = in_pos.zw; \n" "gl_Position = vec4(in_pos.xy, 0, 1); \n" "}\n";

    static const char kFragmentShader[] =
        "#extension GL_OES_EGL_image_external : require\n"
        "precision mediump float;\n" "varying vec2 interp_tc; \n"
        "uniform samplerExternalOES tex; \n" "void main() {\n"
        "gl_FragColor = texture2D(tex, interp_tc);\n" "}\n";

    glEnable(GL_SCISSOR_TEST);
    program = glCreateProgram();

    CreateShader(program, GL_VERTEX_SHADER, kVertexShader,
                 sizeof(kVertexShader));
    CreateShader(program, GL_FRAGMENT_SHADER, kFragmentShader,
                 sizeof(kFragmentShader));

    glLinkProgram(program);
    if (glGetError() != GL_NO_ERROR)
    {
        COMP_ERROR_MSG("Got gl error as " << glGetError());
        return -1;
    }

    glGetProgramiv(program, GL_LINK_STATUS, &result);
    if (!result)
    {
        glGetShaderInfoLog(program, sizeof(log), NULL, log);
        COMP_ERROR_MSG("Error while Linking " << log);
        return -1;
    }

    glUseProgram(program);
    if (glGetError() != GL_NO_ERROR)
    {
        COMP_ERROR_MSG("Got gl error as " << glGetError());
        return -1;
    }

    GLuint vbo; // Store vetex and tex coords
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertexTexBuf), vertexTexBuf, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    pos_location = glGetAttribLocation(program, "in_pos");

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(pos_location, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(pos_location);

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(program, "texSampler"), 0);
    if (glGetError() != GL_NO_ERROR)
    {
        COMP_ERROR_MSG("Got gl error as " << glGetError());
        return -1;
    }
    COMP_DEBUG_MSG("Shaders intialized");
    return 0;
}

int
NvEglRenderer::create_texture()
{
    int viewport[4];

    glGetIntegerv(GL_VIEWPORT, viewport);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glScissor(viewport[0], viewport[1], viewport[2], viewport[3]);

    glGenTextures(1, &texture_id);

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
    return 0;
}

