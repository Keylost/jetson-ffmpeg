/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION. All rights reserved.
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

#include "NvVideoEncoder.h"
#include "NvUtils.h"
#include <fstream>
#include <iostream>
#include <linux/videodev2.h>
#include <malloc.h>
#include <string.h>

#include "nvbuf_utils.h"
#include "NvCudaProc.h"
#include "video_cuda_enc.h"

#define TEST_ERROR(cond, str, label) if(cond) { \
                                        cerr << str << endl; \
                                        error = 1; \
                                        goto label; }

using namespace std;

static void
abort(context_t *ctx)
{
    ctx->got_error = true;
    ctx->enc->abort();
}

static int
write_encoder_output_frame(ofstream * stream, NvBuffer * buffer)
{
    stream->write((char *) buffer->planes[0].data, buffer->planes[0].bytesused);
    return 0;
}

/**
 * Callback function called after capture plane dqbuffer of NvVideoEncoder class.
 * See NvV4l2ElementPlane::dqThread() in sample/common/class/NvV4l2ElementPlane.cpp
 * for details.
 *
 * @param v4l2_buf       : dequeued v4l2 buffer
 * @param buffer         : NvBuffer associated with the dequeued v4l2 buffer
 * @param shared_buffer  : Shared NvBuffer if the queued buffer is shared with
 *                         other elements. Can be NULL.
 * @param arg            : private data set by NvV4l2ElementPlane::startDQThread()
 *
 * @return               : true for success, false for failure (will stop DQThread)
 */
static bool
encoder_capture_plane_dq_callback(struct v4l2_buffer *v4l2_buf, NvBuffer * buffer,
                                  NvBuffer * shared_buffer, void *arg)
{
    context_t *ctx = (context_t *) arg;
    NvVideoEncoder *enc = ctx->enc;

    if (!v4l2_buf)
    {
        cerr << "Failed to dequeue buffer from encoder capture plane" << endl;
        abort(ctx);
        return false;
    }

    write_encoder_output_frame(ctx->out_file, buffer);

    /* qBuffer on the capture plane */
    if (enc->capture_plane.qBuffer(*v4l2_buf, NULL) < 0)
    {
        cerr << "Error while Qing buffer at capture plane" << endl;
        abort(ctx);
        return false;
    }

    /* GOT EOS from encoder. Stop dqthread. */
    if (buffer->planes[0].bytesused == 0)
    {
        return false;
    }

    return true;
}

static void
set_defaults(context_t * ctx)
{
    memset(ctx, 0, sizeof(context_t));

    ctx->bitrate = 4 * 1024 * 1024;
    ctx->fps_n = 30;
    ctx->fps_d = 1;
}

int
main(int argc, char *argv[])
{
    context_t ctx;
    int ret = 0;
    int error = 0;
    bool eos = false;

    set_defaults(&ctx);

    ctx.eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (ctx.eglDisplay == EGL_NO_DISPLAY)
    {
        cout<<"Could not get EGL display connection"<<endl;
        return -1;
    }

    /**
     * Initialize egl, egl maps DMA fd of encoder output plane
     * for CUDA to process (render a black rectangle).
     */
    if (!eglInitialize(ctx.eglDisplay, NULL, NULL))
    {
        cout<<"init EGL display failed"<<endl;
        return -1;
    }

    ret = parse_csv_args(&ctx, argc, argv);
    TEST_ERROR(ret < 0, "Error parsing commandline arguments", cleanup);

    ctx.in_file = new ifstream(ctx.in_file_path);
    TEST_ERROR(!ctx.in_file->is_open(), "Could not open input file", cleanup);

    ctx.out_file = new ofstream(ctx.out_file_path);
    TEST_ERROR(!ctx.out_file->is_open(), "Could not open output file", cleanup);

    ctx.enc = NvVideoEncoder::createVideoEncoder("enc0");
    TEST_ERROR(!ctx.enc, "Could not create encoder", cleanup);

    /**
     * It is necessary that Capture Plane format be set before Output Plane
     * format.
     * It is necessary to set width and height on the capture plane as well.
     */
    ret =
        ctx.enc->setCapturePlaneFormat(ctx.encoder_pixfmt, ctx.width,
                                      ctx.height, 2 * 1024 * 1024);
    TEST_ERROR(ret < 0, "Could not set capture plane format", cleanup);

    ret =
        ctx.enc->setOutputPlaneFormat(V4L2_PIX_FMT_YUV420M, ctx.width,
                                      ctx.height);
    TEST_ERROR(ret < 0, "Could not set output plane format", cleanup);

    ret = ctx.enc->setBitrate(ctx.bitrate);
    TEST_ERROR(ret < 0, "Could not set bitrate", cleanup);

    if (ctx.encoder_pixfmt == V4L2_PIX_FMT_H264)
    {
        ret = ctx.enc->setProfile(V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);
    }
    else
    {
        ret = ctx.enc->setProfile(V4L2_MPEG_VIDEO_H265_PROFILE_MAIN);
    }
    TEST_ERROR(ret < 0, "Could not set encoder profile", cleanup);

    if (ctx.encoder_pixfmt == V4L2_PIX_FMT_H264)
    {
        ret = ctx.enc->setLevel(V4L2_MPEG_VIDEO_H264_LEVEL_5_0);
        TEST_ERROR(ret < 0, "Could not set encoder level", cleanup);
    }

    ret = ctx.enc->setFrameRate(ctx.fps_n, ctx.fps_d);
    TEST_ERROR(ret < 0, "Could not set framerate", cleanup);

    /**
     * Query, Export and Map the output plane buffers so that we can read
     * raw data into the buffers
     */
    ret = ctx.enc->output_plane.setupPlane(V4L2_MEMORY_MMAP, 10, true, false);
    TEST_ERROR(ret < 0, "Could not setup output plane", cleanup);

    /**
     * Query, Export and Map the capture plane buffers so that we can write
     * encoded data from the buffers
     */
    ret = ctx.enc->capture_plane.setupPlane(V4L2_MEMORY_MMAP, 6, true, false);
    TEST_ERROR(ret < 0, "Could not setup capture plane", cleanup);

    /* output plane STREAMON */
    ret = ctx.enc->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in output plane streamon", cleanup);

    /* capture plane STREAMON */
    ret = ctx.enc->capture_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in capture plane streamon", cleanup);

    ctx.enc->capture_plane.
        setDQThreadCallback(encoder_capture_plane_dq_callback);

    /**
     * startDQThread starts a thread internally which calls the
     * encoder_capture_plane_dq_callback whenever a buffer is dequeued
     * on the plane
     */
    ctx.enc->capture_plane.startDQThread(&ctx);

    /* Enqueue all the empty capture plane buffers */
    for (uint32_t i = 0; i < ctx.enc->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        ret = ctx.enc->capture_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error while queueing buffer at capture plane" << endl;
            abort(&ctx);
            goto cleanup;
        }
    }

    /* Read video frame and queue all the output plane buffers */
    for (uint32_t i = 0; i < ctx.enc->output_plane.getNumBuffers() &&
            !ctx.got_error; i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer = ctx.enc->output_plane.getNthBuffer(i);
        int fd;
        void **dat;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        if (read_video_frame(ctx.in_file, *buffer) < 0)
        {
            cerr << "Could not read complete frame from input file" << endl;
            v4l2_buf.m.planes[0].bytesused = 0;
        }
        /**
         * buffer is touched by CPU in read_video_frame(), so NvBufferMemSyncForDevice()
         * is needed to flash cached data to memory.
         */
        fd = buffer->planes[0].fd;
        for (uint32_t j = 0 ; j < buffer->n_planes ; j++)
        {
            dat = (void **)&buffer->planes[j].data;
            ret = NvBufferMemSyncForDevice (fd, j, dat);
            if (ret < 0)
            {
                cerr << "Error while NvBufferMemSyncForDevice at output plane" << endl;
                abort(&ctx);
                goto cleanup;
            }
        }

        /* map DMA fd to eglImage for CUDA processing */
        ctx.eglimg = NvEGLImageFromFd(ctx.eglDisplay, buffer->planes[0].fd);
        /* render rectangle by CUDA */
        HandleEGLImage(&ctx.eglimg);
        /* release eglImage */
        NvDestroyEGLImage(ctx.eglDisplay, ctx.eglimg);

        ret = ctx.enc->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error while queueing buffer at output plane" << endl;
            abort(&ctx);
            goto cleanup;
        }

        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            cerr << "File read complete." << endl;
            eos = true;
            break;
        }
    }

    /* Keep reading input till EOS is reached */
    while (!ctx.got_error && !ctx.enc->isInError() && !eos)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;
        int fd;
        void **dat;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;

        if (ctx.enc->output_plane.dqBuffer(v4l2_buf, &buffer, NULL, 10) < 0)
        {
            cerr << "ERROR while DQing buffer at output plane" << endl;
            abort(&ctx);
            goto cleanup;
        }

        if (read_video_frame(ctx.in_file, *buffer) < 0)
        {
            cerr << "Could not read complete frame from input file" << endl;
            v4l2_buf.m.planes[0].bytesused = 0;
        }
        /**
         * buffer is touched by CPU in read_video_frame(), so NvBufferMemSyncForDevice()
         * is needed to flash cached data to memory.
         */
        fd = buffer->planes[0].fd;
        for (uint32_t j = 0 ; j < buffer->n_planes ; j++)
        {
            dat = (void **)&buffer->planes[j].data;
            ret = NvBufferMemSyncForDevice (fd, j, dat);
            if (ret < 0)
            {
                cerr << "Error while NvBufferMemSyncForDevice at output plane" << endl;
                abort(&ctx);
                goto cleanup;
            }
        }

        /* map DMA fd to eglImage for CUDA processing */
        ctx.eglimg = NvEGLImageFromFd(ctx.eglDisplay, buffer->planes[0].fd);
        /* render rectangle by CUDA */
        HandleEGLImage(&ctx.eglimg);
        /* release eglImage */
        NvDestroyEGLImage(ctx.eglDisplay, ctx.eglimg);

        ret = ctx.enc->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error while queueing buffer at output plane" << endl;
            abort(&ctx);
            goto cleanup;
        }

        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            cerr << "File read complete." << endl;
            eos = true;
            break;
        }
    }

    /**
     * Wait till capture plane DQ Thread finishes
     * i.e. all the capture plane buffers are dequeued
     */
    ctx.enc->capture_plane.waitForDQThread(2000);

cleanup:
    if (ctx.enc && ctx.enc->isInError())
    {
        cerr << "Encoder is in error" << endl;
        error = 1;
    }
    if (ctx.got_error)
    {
        error = 1;
    }

    delete ctx.enc;
    delete ctx.in_file;
    delete ctx.out_file;

    free(ctx.in_file_path);
    free(ctx.out_file_path);

    if (!eglTerminate(ctx.eglDisplay))
    {
        cout<<"ERROR eglTerminate failed"<<endl;
        error = 1;
    }

    if (!eglReleaseThread())
    {
        cout<<"ERROR eglReleaseThread failed"<<endl;
        error = 1;
    }

    if (error)
    {
        cout << "App run failed" << endl;
    }
    else
    {
        cout << "App run was successful" << endl;
    }
    return -error;
}
