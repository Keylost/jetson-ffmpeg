/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __VIDEO_DECODE_DRM_H__
#define __VIDEO_DECODE_DRM_H__

#include "NvVideoDecoder.h"
#include "NvVideoConverter.h"
#include "NvEglRenderer.h"
#include "NvDrmRenderer.h"
#include <queue>
#include <fstream>
#include <pthread.h>

#define MAX_BUFFERS 32

typedef struct
{
    /* Video decoder */
    NvVideoDecoder *dec;
    uint32_t decoder_pixfmt;
    char *in_file_path;
    std::ifstream *in_file;

    /* LibDrm renderer */
    NvDrmRenderer *drm_renderer;
    uint32_t crtc;
    uint32_t connector;
    bool disable_video;
    bool disable_ui;
    int console_fd;
    int active_vt;
    /* Window location of video stream */
    uint32_t window_height;
    uint32_t window_width;
    uint32_t window_x;
    uint32_t window_y;
    uint32_t fps;



    /* Thread used to stream video */
    pthread_t dec_capture_loop;
    bool got_error;
    bool got_eos;
    bool streamHDR;

    /* Thread used to draw UI */
    pthread_t ui_renderer_loop;
    /* Ensure all the threads, except UI thread, have completed */
    bool got_exit;

    /* The iteration of stress test */
    uint32_t stress_iteration;

    /* Enable data profile */
    bool stats;

    int numCapBuffers;
    int dec_fd[MAX_BUFFERS];
    int numRenderBuffers;
    int render_fd[MAX_BUFFERS];
    uint32_t conv_out_colorspace;
    char *out_file_path;
    std::ofstream *out_file;
} context_t;

typedef struct
{
    unsigned int width;
    unsigned int height;

} resolution;

int parse_csv_args(context_t * ctx, int argc, char *argv[]);
void abort(context_t *ctx);
#endif /* #ifndef __VIDEO_DECODE_DRM_H__ */
