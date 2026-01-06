/*
 * Copyright (c) 2016-2023, NVIDIA CORPORATION. All rights reserved.
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

#include "NvUtils.h"
#include <errno.h>
#include <fstream>
#include <iostream>
#include <malloc.h>
#include <string.h>
#include <unistd.h>

#include "jpeg_encode.h"

#define TEST_ERROR(cond, str, label) if(cond) { \
                                        cerr << str << endl; \
                                        error = 1; \
                                        goto label; }

#define PERF_LOOP   300

using namespace std;

static void
abort(context_t * ctx)
{
    ctx->got_error = true;
    ctx->conv->abort();
}

/**
 * Callback function called after capture plane dqbuffer of NvVideoConverter class.
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
conv_capture_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                   NvBuffer * buffer, NvBuffer * shared_buffer,
                                   void *arg)
{
    context_t *ctx = (context_t *) arg;
    unsigned long out_buf_size = ctx->in_width * ctx->in_height * 3 / 2;
    unsigned char *out_buf = (unsigned char *)malloc(out_buf_size);
    int iterator_num = ctx->perf ? PERF_LOOP : 1;
    int ret;

    if (!v4l2_buf)
    {
        cerr << "Failed to dequeue buffer from conv capture plane" << endl;
        abort(ctx);
        free(out_buf);
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused > 0)
    {
        for (int i = 0; i < iterator_num; ++i)
        {
            ret = ctx->jpegenc->encodeFromFd(buffer->planes[0].fd, JCS_YCbCr, &out_buf,
                out_buf_size, ctx->quality);
            if (ret < 0)
            {
                cerr << "Error while encoding from fd" << endl;
                ctx->got_error = true;
                break;
            }
        }
        if (ret >= 0)
        {
            ctx->out_file->write((char *) out_buf, out_buf_size);
        }
    }

    free(out_buf);

    return false;
}

static void
set_defaults(context_t * ctx)
{
    memset(ctx, 0, sizeof(context_t));
    ctx->perf = false;
    ctx->use_fd = true;
    ctx->in_pixfmt = V4L2_PIX_FMT_YUV420M;
    ctx->stress_test = 1;
    ctx->quality = 75;
}

/**
 * Class NvJPEGEncoder encodes YUV420 image to JPEG.
 * NvJPEGEncoder::encodeFromBuffer() encodes from software buffer memory
 * which can be access by CPU directly.
 * NvJPEGEncoder::encodeFromFd() encodes from hardware buffer memory which is faster
 * than NvJPEGEncoder::encodeFromBuffer() since the latter involves conversion
 * from software buffer memory to hardware buffer memory.
 *
 * When using NvJPEGEncoder::encodeFromFd(), class NvVideoConverter is used to
 * convert MMAP buffer (CPU buffer holding YUV420 image) to hardware buffer memory
 * (DMA buffer fd). There may be YUV420 to NV12 conversion depends on commandline
 * argument.
 */
static int
jpeg_encode_proc(context_t& ctx, int argc, char *argv[])
{
    int ret = 0;
    int error = 0;
    int iterator_num = 1;

    set_defaults(&ctx);

    ret = parse_csv_args(&ctx, argc, argv);
    TEST_ERROR(ret < 0, "Error parsing commandline arguments", cleanup);

    ctx.in_file = new ifstream(ctx.in_file_path);
    TEST_ERROR(!ctx.in_file->is_open(), "Could not open input file", cleanup);

    ctx.out_file = new ofstream(ctx.out_file_path);
    TEST_ERROR(!ctx.out_file->is_open(), "Could not open output file", cleanup);

    ctx.jpegenc = NvJPEGEncoder::createJPEGEncoder("jpenenc");
    TEST_ERROR(!ctx.jpegenc, "Could not create Jpeg Encoder", cleanup);

    if (ctx.perf)
    {
        iterator_num = PERF_LOOP;
        ctx.jpegenc->enableProfiling();
    }

    ctx.jpegenc->setCropRect(ctx.crop_left, ctx.crop_top,
            ctx.crop_width, ctx.crop_height);

    if(ctx.scaled_encode)
    {
      ctx.jpegenc->setScaledEncodeParams(ctx.scale_width, ctx.scale_height);
    }

    /**
     * Case 1:
     * Read YUV420 image from file system to CPU buffer, encode by
     * encodeFromBuffer() then write to file system.
     */
    if (!ctx.use_fd)
    {
        unsigned long out_buf_size = ctx.in_width * ctx.in_height * 3 / 2;
        unsigned char *out_buf = (unsigned char *)malloc(out_buf_size);

        NvBuffer buffer(V4L2_PIX_FMT_YUV420M, ctx.in_width,
                ctx.in_height, 0);

        buffer.allocateMemory();

        ret = read_video_frame(ctx.in_file, buffer);
        TEST_ERROR(ret < 0, "Could not read a complete frame from file",
                cleanup);

        for (int i = 0; i < iterator_num; ++i)
        {
            ret = ctx.jpegenc->encodeFromBuffer(buffer, JCS_YCbCr, &out_buf,
                    out_buf_size, ctx.quality);
            TEST_ERROR(ret < 0, "Error while encoding from buffer", cleanup);
        }

        ctx.out_file->write((char *) out_buf, out_buf_size);
        free(out_buf);

        goto cleanup;
    }

    /**
     * Case 2:
     * Read YUV420 image from file system to CPU buffer, convert to hardware
     * buffer memory (DMA buffer fd), encode by encodeFromFd() then write to
     * file system.
     * Note:
     *     While converting to hardware buffer, NvVideoConverter may convert
     *     YUV420 to NV12 depends on ctx.in_pixfmt.
     */
    ctx.conv = NvVideoConverter::createVideoConverter("conv");
    TEST_ERROR(!ctx.conv, "Could not create Video Converter", cleanup);

    /* Set conv output plane format */
    ret =
        ctx.conv->setOutputPlaneFormat(V4L2_PIX_FMT_YUV420M, ctx.in_width,
                                       ctx.in_height,
                                       V4L2_NV_BUFFER_LAYOUT_PITCH);
    TEST_ERROR(ret < 0, "Could not set output plane format for conv", cleanup);

    /* Set conv capture plane format, YUV420 or NV12 */
    ret =
        ctx.conv->setCapturePlaneFormat(ctx.in_pixfmt, ctx.in_width,
                                        ctx.in_height,
                                        V4L2_NV_BUFFER_LAYOUT_BLOCKLINEAR);
    TEST_ERROR(ret < 0, "Could not set capture plane format for conv", cleanup);

    /* REQBUF, EXPORT and MAP conv output plane buffers */
    ret = ctx.conv->output_plane.setupPlane(V4L2_MEMORY_MMAP, 1, true, false);
    TEST_ERROR(ret < 0, "Error while setting up output plane for conv",
               cleanup);

    /**
     * REQBUF and EXPORT conv capture plane buffers
     * No need to MAP since buffer will be shared to next component
     * and not read in application
     */
    ret =
        ctx.conv->capture_plane.setupPlane(V4L2_MEMORY_MMAP, 1,
                                            !ctx.use_fd, false);
    TEST_ERROR(ret < 0, "Error while setting up capture plane for conv",
               cleanup);

    /* conv output plane STREAMON */
    ret = ctx.conv->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in output plane streamon for conv", cleanup);

    /* conv capture plane STREAMON */
    ret = ctx.conv->capture_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in capture plane streamon for conv", cleanup);

    /**
     * Register callback for dequeue thread on conv capture plane, this callback
     * will encode YUV420 or NV12 image to JPEG and write to file system.
     */
    ctx.conv->
        capture_plane.setDQThreadCallback(conv_capture_dqbuf_thread_callback);

    // Start threads to dequeue buffers on conv capture plane
    ctx.conv->capture_plane.startDQThread(&ctx);

    /**
     * Enqueue all empty conv capture plane buffers, actually in this case,
     * 1 buffer will be enqueued.
     */
    for (uint32_t i = 0; i < ctx.conv->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        ret = ctx.conv->capture_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error while queueing buffer at conv capture plane" << endl;
            abort(&ctx);
            goto cleanup;
        }
    }

    /**
     * Read YUV420 image to conv output plane buffer and enqueue so conv can
     * start processing.
     */
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer = ctx.conv->output_plane.getNthBuffer(0);

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = 0;
        v4l2_buf.m.planes = planes;

        if (read_video_frame(ctx.in_file, *buffer) < 0)
        {
            cerr << "Could not read a complete frame from file" << endl;
            v4l2_buf.m.planes[0].bytesused = 0;
        }

        ret = ctx.conv->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error while queueing buffer at conv output plane" << endl;
            abort(&ctx);
            goto cleanup;
        }
    }

    /* Wait till all capture plane buffers on conv are dequeued */
    ctx.conv->capture_plane.waitForDQThread(2000);

cleanup:
    if (ctx.perf)
    {
        ctx.jpegenc->printProfilingStats(cout);
    }

    if (ctx.conv && ctx.conv->isInError())
    {
        cerr << "VideoConverter is in error" << endl;
        error = 1;
    }

    if (ctx.got_error)
    {
        error = 1;
    }

    delete ctx.in_file;
    delete ctx.out_file;
    /**
     * Destructors do all the cleanup, unmapping and deallocating buffers
     * and calling v4l2_close on fd
     */
    delete ctx.conv;
    delete ctx.jpegenc;

    free(ctx.in_file_path);
    free(ctx.out_file_path);

    return -error;
}

int
main(int argc, char *argv[])
{
    context_t ctx;
    int ret = 0;
    /* save iterator number */
    int iterator_num = 0;

    do
    {
        ret = jpeg_encode_proc(ctx, argc, argv);
        iterator_num++;
    } while((ctx.stress_test != iterator_num) && ret == 0);

    if (ret)
    {
        cout << "App run failed" << endl;
    }
    else
    {
        cout << "App run was successful" << endl;
    }
    return ret;
}
