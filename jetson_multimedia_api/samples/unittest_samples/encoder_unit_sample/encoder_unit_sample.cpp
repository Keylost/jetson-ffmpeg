/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
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
 * Execution command
 * ./encode_sample raw_file.yuv (int)width (int)height encoded_file.264
 * Eg: ./encode_sample test_h264_raw.yuv 1920 1080 encoded_h264.264
 **/

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <stdint.h>
#include <unistd.h>
#include <cstdlib>
#include <libv4l2.h>
#include <linux/videodev2.h>
#include <malloc.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>

#include "nvbuf_utils.h"
#include "v4l2_nv_extensions.h"

using namespace std;

#include "encoder_unit_sample.hpp"

/**
 *
 * V4L2 H264 Video Encoder Sample
 *
 * The video encoder device node is
 *     /dev/nvhost-msenc
 *
 * In this sample:
 * ## Pixel Formats
 * OUTPUT PLANE         | CAPTURE PLANE
 * :----------------:   | :----------------:
 * V4L2_PIX_FMT_YUV420M | V4L2_PIX_FMT_H264
 *
 * ## Memory Type
 *            | OUTPUT PLANE        | CAPTURE PLANE
 * :--------: | :----------:        | :-----------:
 * MEMORY     | V4L2_MEMORY_MMAP    | V4L2_MEMORY_MMAP
 *
 * ## Supported Controls
 * - #V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT
 * - V4L2_CID_MIN_BUFFERS_FOR_CAPTURE (Get the minimum buffers to be allocated
 * on capture plane.
 * Read-only. Valid after #V4L2_EVENT_RESOLUTION_CHANGE)
 *
 * ## Supported Events
 * Event                         | Purpose
 * ----------------------------- | :----------------------------:
 * #V4L2_EVENT_EOS               | End of Stream detected.
 *
 * ## Opening the Encoder
 * The encoder device node is opened through the v4l2_open IOCTL call.
 * After opening the device, the application calls VIDIOC_QUERYCAP to identify
 * the driver capabilities.
 *
 * ## Subscribing events and setting up the planes
 * The application subscribes to the V4L2_EVENT_EOS event,
 * to detect the end of stream and handle the plane buffers
 * accordingly.
 * It calls VIDIOC_S_FMT to setup the formats required on
 * OUTPUT PLANE and CAPTURE PLANE for the data
 * negotiation between the former and the driver.
 * It is necessary to set capture plane format before the output plane format
 * along with the frame width and height
 *
 * ## Setting Controls
 * The application gets/sets the properties of the encoder by setting
 * the controls, calling VIDIOC_S_EXT_CTRLS, VIDIOC_G_CTRL.
 *
 * ## Buffer Management
 * Buffers are requested on the OUTPUT PLANE by the application, calling
 * VIDIOC_REQBUFS. The actual buffers allocated by the encoder are then
 * queried and exported as FD for the DMA-mapped buffer while mapped
 * for Mmaped buffer.
 * Status STREAMON is called on both planes to signal the encoder for
 * processing.
 *
 * Application continuously queues the raw data in the allocated
 * OUTPUT PLANE buffer and dequeues the next empty buffer fed into the
 * encoder.
 * The encoder encodes the raw buffer and signals a successful dequeue
 * on the capture plane, from where the data of v4l2_buffer dequeued is
 * dumped as an encoded bitstream.
 *
 * The encoding thread blocks on the DQ buffer call, which returns either after
 * a successful encoded bitstream or after a specific timeout.
 *
 * ## EOS Handling
 * For sending EOS and receiving EOS from the encoder, the application must
 * - Send EOS to the encoder by queueing on the output plane a buffer with
 * bytesused = 0 for the 0th plane (`v4l2_buffer.m.planes[0].bytesused = 0`).
 * - Dequeues buffers on the output plane until it gets a buffer with bytesused = 0
 * for the 0th plane (`v4l2_buffer.m.planes[0].bytesused == 0`)
 * - Dequeues buffers on the capture plane until it gets a buffer with bytesused = 0
 * for the 0th plane.
 * After the last buffer on the capture plane is dequeued, set STREAMOFF on both
 * planes and destroy the allocated buffers.
 *
 */

#define CHECK_ERROR(condition, error_str, label) if (condition) { \
                                                        cerr << error_str << endl; \
                                                        ctx.in_error = 1; \
                                                        goto label; }

Buffer::Buffer(enum v4l2_buf_type buf_type, enum v4l2_memory memory_type,
        uint32_t index)
        :buf_type(buf_type),
         memory_type(memory_type),
         index(index)
{
    uint32_t i;

    memset(planes, 0, sizeof(planes));

    mapped = false;
    n_planes = 1;
    for (i = 0; i < n_planes; i++)
    {
        this->planes[i].fd = -1;
        this->planes[i].data = NULL;
        this->planes[i].bytesused = 0;
        this->planes[i].mem_offset = 0;
        this->planes[i].length = 0;
        this->planes[i].fmt.sizeimage = 0;
    }
}

Buffer::Buffer(enum v4l2_buf_type buf_type, enum v4l2_memory memory_type,
        uint32_t n_planes, BufferPlaneFormat * fmt, uint32_t index)
        :buf_type(buf_type),
         memory_type(memory_type),
         index(index),
         n_planes(n_planes)
{
    uint32_t i;

    mapped = false;

    memset(planes, 0, sizeof(planes));
    for (i = 0; i < n_planes; i++)
    {
        this->planes[i].fd = -1;
        this->planes[i].fmt = fmt[i];
    }
}

Buffer::~Buffer()
{
    if (mapped)
    {
        unmap();
    }
}

int
Buffer::map()
{
    uint32_t j;

    if (memory_type != V4L2_MEMORY_MMAP)
    {
        cout << "Buffer " << index << "already mapped" << endl;
        return -1;
    }

    if (mapped)
    {
        cout << "Buffer " << index << "already mapped" << endl;
        return 0;
    }

    for (j = 0; j < n_planes; j++)
    {
        if (planes[j].fd == -1)
        {
            return -1;
        }

        planes[j].data = (unsigned char *) mmap(NULL,
                                                planes[j].length,
                                                PROT_READ | PROT_WRITE,
                                                MAP_SHARED,
                                                planes[j].fd,
                                                planes[j].mem_offset);
        if (planes[j].data == MAP_FAILED)
        {
            cout << "Could not map buffer " << index << ", plane " << j << endl;
            return -1;
        }

    }
    mapped = true;
    return 0;
}

void
Buffer::unmap()
{
    if (memory_type != V4L2_MEMORY_MMAP || !mapped)
    {
        cout << "Cannot Unmap Buffer " << index <<
                ". Only mapped MMAP buffer can be unmapped" << endl;
        return;
    }

    for (uint32_t j = 0; j < n_planes; j++)
    {
        if (planes[j].data)
        {
            munmap(planes[j].data, planes[j].length);
        }
        planes[j].data = NULL;
    }
    mapped = false;
}

int
Buffer::fill_buffer_plane_format(uint32_t *num_planes,
        Buffer::BufferPlaneFormat *planefmts,
        uint32_t width, uint32_t height, uint32_t raw_pixfmt)
{
    switch (raw_pixfmt)
    {
        case V4L2_PIX_FMT_YUV420M:
            *num_planes = 3;

            planefmts[0].width = width;
            planefmts[1].width = width / 2;
            planefmts[2].width = width / 2;

            planefmts[0].height = height;
            planefmts[1].height = height / 2;
            planefmts[2].height = height / 2;

            planefmts[0].bytesperpixel = 1;
            planefmts[1].bytesperpixel = 1;
            planefmts[2].bytesperpixel = 1;
            break;
        case V4L2_PIX_FMT_NV12M:
            *num_planes = 2;

            planefmts[0].width = width;
            planefmts[1].width = width / 2;

            planefmts[0].height = height;
            planefmts[1].height = height / 2;

            planefmts[0].bytesperpixel = 1;
            planefmts[1].bytesperpixel = 2;
            break;
        default:
            cout << "Unsupported pixel format " << raw_pixfmt << endl;
            return -1;
    }
    return 0;
}

static int
read_video_frame(ifstream * stream, Buffer & buffer)
{
    uint32_t i, j;
    char *data;

    for (i = 0; i < buffer.n_planes; i++)
    {
        Buffer::BufferPlane &plane = buffer.planes[i];
        streamsize bytes_to_read =
            plane.fmt.bytesperpixel * plane.fmt.width;
        data = (char *) plane.data;
        plane.bytesused = 0;
        /* It is necessary to set bytesused properly,
        ** so that encoder knows how
        ** many bytes in the buffer to be read.
        */
        for (j = 0; j < plane.fmt.height; j++)
        {
            stream->read(data, bytes_to_read);
            if (stream->gcount() < bytes_to_read)
                return -1;
            data += plane.fmt.stride;
        }
        plane.bytesused = plane.fmt.stride * plane.fmt.height;
    }
    return 0;
}

static int
write_encoded_frame(ofstream * stream, Buffer * buffer)
{
    stream->write((char *)buffer->planes[0].data, buffer->planes[0].bytesused);
    return 0;
}

static int
wait_for_dqthread(context_t& ctx, uint32_t max_wait_ms)
{
    struct timespec waiting_time;
    struct timeval now;
    int ret_val = 0;
    int dq_return = 0;

    gettimeofday(&now, NULL);

    waiting_time.tv_nsec = (now.tv_usec + (max_wait_ms % 1000) * 1000L) * 1000L;
    waiting_time.tv_sec = now.tv_sec + max_wait_ms / 1000 +
        waiting_time.tv_nsec / 1000000000L;
    waiting_time.tv_nsec = waiting_time.tv_nsec % 1000000000L;

    pthread_mutex_lock(&ctx.queue_lock);
    while (ctx.dqthread_running)
    {
        dq_return = pthread_cond_timedwait(&ctx.queue_cond, &ctx.queue_lock,
            &waiting_time);
        if (dq_return == ETIMEDOUT)
        {
            ret_val = -1;
            break;
        }
    }

    pthread_mutex_unlock(&ctx.queue_lock);

    if (dq_return == 0)
    {
        pthread_join(ctx.enc_dq_thread, NULL);
        ctx.enc_dq_thread = 0;
    }
    else
    {
        cerr << "Time out waiting for dqthread" << endl;
        ctx.in_error = 1;
    }
    return ret_val;
}

static int
set_capture_plane_format(context_t& ctx, uint32_t sizeimage)
{
    int ret_val = 0;
    struct v4l2_format format;

    memset(&format, 0, sizeof (struct v4l2_format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    format.fmt.pix_mp.pixelformat = ctx.encode_pixfmt;
    format.fmt.pix_mp.width = ctx.width;
    format.fmt.pix_mp.height = ctx.height;
    format.fmt.pix_mp.num_planes = 1;
    format.fmt.pix_mp.plane_fmt[0].sizeimage = sizeimage;

    ret_val = v4l2_ioctl (ctx.fd, VIDIOC_S_FMT, &format);

    if (!ret_val)
    {
        ctx.capplane_num_planes = format.fmt.pix_mp.num_planes;
        for (uint32_t i = 0; i < ctx.capplane_num_planes; ++i)
        {
            ctx.capplane_planefmts[i].stride =
                format.fmt.pix_mp.plane_fmt[i].bytesperline;
            ctx.capplane_planefmts[i].sizeimage =
                format.fmt.pix_mp.plane_fmt[i].sizeimage;
        }
    }

    return ret_val;
}

static int
set_output_plane_format(context_t& ctx)
{
    struct v4l2_format format;
    int ret_val = 0;
    uint32_t num_bufferplanes;
    Buffer::BufferPlaneFormat planefmts[MAX_PLANES];

    if (ctx.raw_pixfmt != V4L2_PIX_FMT_YUV420M)
    {
        cerr << "Only V4L2_PIX_FMT_YUV420M is supported" << endl;
        return -1;
    }

    Buffer::fill_buffer_plane_format(&num_bufferplanes, planefmts, ctx.width,
            ctx.height, ctx.raw_pixfmt);

    ctx.outplane_num_planes = num_bufferplanes;
    for (uint32_t i = 0; i < num_bufferplanes; ++i)
    {
        ctx.outplane_planefmts[i] = planefmts[i];
    }
    memset(&format, 0, sizeof (struct v4l2_format));
    format.type = ctx.outplane_buf_type;
    format.fmt.pix_mp.width = ctx.width;
    format.fmt.pix_mp.height = ctx.height;
    format.fmt.pix_mp.pixelformat = ctx.raw_pixfmt;
    format.fmt.pix_mp.num_planes = num_bufferplanes;

    ret_val = v4l2_ioctl(ctx.fd, VIDIOC_S_FMT, &format);
    if (!ret_val)
    {
        ctx.outplane_num_planes = format.fmt.pix_mp.num_planes;
        for (uint32_t j = 0; j < ctx.outplane_num_planes; j++)
        {
            ctx.outplane_planefmts[j].stride =
                format.fmt.pix_mp.plane_fmt[j].bytesperline;
            ctx.outplane_planefmts[j].sizeimage =
                format.fmt.pix_mp.plane_fmt[j].sizeimage;
        }
    }

    return ret_val;
}

static int
req_buffers_on_capture_plane(context_t * ctx, enum v4l2_buf_type buf_type,
        enum v4l2_memory mem_type, int num_buffers)
{
    struct v4l2_requestbuffers reqbuffers;
    int ret_val = 0;
    memset (&reqbuffers, 0, sizeof (struct v4l2_requestbuffers));

    reqbuffers.count = num_buffers;
    reqbuffers.memory = mem_type;
    reqbuffers.type = buf_type;

    ret_val = v4l2_ioctl (ctx->fd, VIDIOC_REQBUFS, &reqbuffers);
    if (ret_val)
        return ret_val;

    if (reqbuffers.count)
    {
        ctx->capplane_buffers = new Buffer *[reqbuffers.count];
        for (uint32_t i = 0; i < reqbuffers.count; ++i)
        {
            ctx->capplane_buffers[i] = new Buffer (buf_type, mem_type,
                ctx->capplane_num_planes, ctx->capplane_planefmts, i);
        }
    }
    else
    {
        for (uint32_t i = 0; i < ctx->capplane_num_buffers; ++i)
        {
            delete ctx->capplane_buffers[i];
        }
        delete[] ctx->capplane_buffers;
        ctx->capplane_buffers = NULL;
    }
    ctx->capplane_num_buffers = reqbuffers.count;

    return ret_val;
}

static int
req_buffers_on_output_plane(context_t * ctx, enum v4l2_buf_type buf_type,
        enum v4l2_memory mem_type, int num_buffers)
{
    struct v4l2_requestbuffers reqbuffers;
    int ret_val = 0;
    memset (&reqbuffers, 0, sizeof (struct v4l2_requestbuffers));

    reqbuffers.count = num_buffers;
    reqbuffers.memory = mem_type;
    reqbuffers.type = buf_type;

    ret_val = v4l2_ioctl (ctx->fd, VIDIOC_REQBUFS, &reqbuffers);
    if (ret_val)
        return ret_val;

    if (reqbuffers.count)
    {
        ctx->outplane_buffers = new Buffer *[reqbuffers.count];
        for (uint32_t i = 0; i < reqbuffers.count; ++i)
        {
            ctx->outplane_buffers[i] = new Buffer (buf_type, mem_type,
                ctx->outplane_num_planes, ctx->outplane_planefmts, i);
        }
    }
    else
    {
        for (uint32_t i = 0; i < ctx->outplane_num_buffers; ++i)
        {
            delete ctx->outplane_buffers[i];
        }
        delete[] ctx->outplane_buffers;
        ctx->outplane_buffers = NULL;
    }
    ctx->outplane_num_buffers = reqbuffers.count;

    return ret_val;
}

static int
subscribe_event(int fd, uint32_t type, uint32_t id, uint32_t flags)
{
    struct v4l2_event_subscription sub;
    int ret_val;

    memset(&sub, 0, sizeof (struct v4l2_event_subscription));

    sub.type = type;
    sub.id = id;
    sub.flags = flags;

    ret_val = v4l2_ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);

    return ret_val;
}

static int
q_buffer(context_t * ctx, struct v4l2_buffer &v4l2_buf, Buffer * buffer,
    enum v4l2_buf_type buf_type, enum v4l2_memory memory_type, int num_planes)
{
    int ret_val;
    uint32_t j;

    pthread_mutex_lock (&ctx->queue_lock);
    v4l2_buf.type = buf_type;
    v4l2_buf.memory = memory_type;
    v4l2_buf.length = num_planes;

    switch (memory_type)
    {
        case V4L2_MEMORY_MMAP:
            for (j = 0; j < buffer->n_planes; ++j)
            {
                v4l2_buf.m.planes[j].bytesused =
                buffer->planes[j].bytesused;
            }
            break;
        case V4L2_MEMORY_DMABUF:
            break;
        default:
            pthread_cond_broadcast (&ctx->queue_cond);
            pthread_mutex_unlock (&ctx->queue_lock);
            return -1;
    }

    ret_val = v4l2_ioctl (ctx->fd, VIDIOC_QBUF, &v4l2_buf);

    if (!ret_val)
    {
        switch (v4l2_buf.type)
        {
            case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
                ctx->num_queued_outplane_buffers++;
                break;
            case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
                ctx->num_queued_capplane_buffers++;
                break;
            default:
                cerr << "Buffer Type not supported" << endl;
        }
        pthread_cond_broadcast (&ctx->queue_cond);
    }
    pthread_mutex_unlock (&ctx->queue_lock);

    return ret_val;
}

static int
dq_buffer(context_t * ctx, struct v4l2_buffer &v4l2_buf, Buffer ** buffer,
    enum v4l2_buf_type buf_type, enum v4l2_memory memory_type, uint32_t num_retries)
{
    int ret_val = 0;
    bool is_in_error = false;
    v4l2_buf.type = buf_type;
    v4l2_buf.memory = memory_type;

    do
    {
        ret_val = v4l2_ioctl (ctx->fd, VIDIOC_DQBUF, &v4l2_buf);

        if (ret_val == 0)
        {
            pthread_mutex_lock(&ctx->queue_lock);
            switch(v4l2_buf.type)
            {
                case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
                    if (buffer)
                        *buffer = ctx->outplane_buffers[v4l2_buf.index];
                    for (uint32_t j = 0; j < ctx->outplane_buffers[v4l2_buf.index]->n_planes; j++)
                    {
                        ctx->outplane_buffers[v4l2_buf.index]->planes[j].bytesused =
                        v4l2_buf.m.planes[j].bytesused;
                    }
                    ctx->num_queued_outplane_buffers--;
                    break;

                case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
                    if (buffer)
                        *buffer = ctx->capplane_buffers[v4l2_buf.index];
                    for (uint32_t j = 0; j < ctx->capplane_buffers[v4l2_buf.index]->n_planes; j++)
                    {
                        ctx->capplane_buffers[v4l2_buf.index]->planes[j].bytesused =
                        v4l2_buf.m.planes[j].bytesused;
                    }
                    ctx->num_queued_capplane_buffers--;
                    break;

                default:
                    cout << "Invaild buffer type" << endl;
            }
            pthread_cond_broadcast(&ctx->queue_cond);
            pthread_mutex_unlock(&ctx->queue_lock);
        }
        else if (errno == EAGAIN)
        {
            pthread_mutex_lock(&ctx->queue_lock);
            if (v4l2_buf.flags & V4L2_BUF_FLAG_LAST)
            {
                pthread_mutex_unlock(&ctx->queue_lock);
                break;
            }
            pthread_mutex_unlock(&ctx->queue_lock);

            if (num_retries-- == 0)
            {
                // Resource temporarily unavailable.
                cout << "Resource temporarily unavailable" << endl;
                break;
            }
        }
        else
        {
            is_in_error = true;
            break;
        }
    }
    while (ret_val && !is_in_error);

    return ret_val;
}

static void *
dq_thread(void *arg)
{
    context_t *ctx = (context_t *)arg;
    bool stop_dqthread = false;

    while (!stop_dqthread)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        Buffer *buffer = new Buffer(ctx->capplane_buf_type,
                ctx->capplane_mem_type, 0);
        bool ret_val;

        memset(&v4l2_buf, 0, sizeof (struct v4l2_buffer));
        memset(planes, 0, MAX_PLANES * sizeof (struct v4l2_plane));
        v4l2_buf.m.planes = planes;
        v4l2_buf.length = ctx->capplane_num_planes;

        if (dq_buffer(ctx, v4l2_buf, &buffer, ctx->capplane_buf_type,
                ctx->capplane_mem_type, -1) < 0)
        {
            if (errno != EAGAIN)
            {
                ctx->in_error = true;
            }

            if (errno != EAGAIN || ctx->capplane_streamon)
                ret_val = capture_plane_callback(NULL, NULL, ctx);

            if (!ctx->capplane_streamon)
                break;
        }
        else
        {
            ret_val = capture_plane_callback(&v4l2_buf, buffer, ctx);
        }
        if (!ret_val)
        {
            break;
        }
    }
    stop_dqthread = true;

    pthread_mutex_lock(&ctx->queue_lock);
    ctx->dqthread_running = false;
    pthread_cond_broadcast(&ctx->queue_cond);
    pthread_mutex_unlock(&ctx->queue_lock);

    return NULL;
}

static bool
capture_plane_callback(struct v4l2_buffer *v4l2_buf, Buffer * buffer, void *arg)
{
    context_t *ctx = (context_t *)arg;

    if (v4l2_buf == NULL)
    {
        cout << "Error while DQing buffer from capture plane" << endl;
        ctx->in_error = 1;
        return false;
    }

    if (buffer->planes[0].bytesused == 0)
    {
        cout << "Got 0 size buffer in capture" << endl;
        return false;
    }

    write_encoded_frame(ctx->output_file, buffer);

    if (q_buffer(ctx, *v4l2_buf, buffer, ctx->capplane_buf_type, ctx->capplane_mem_type,
            ctx->capplane_num_planes) < 0)
    {
        cerr << "Error while Qing buffer at capture plane" <<  endl;
        ctx->in_error = 1;
        return false;
    }

    return true;
}

static int
encoder_process_blocking(context_t& ctx)
{
    int ret_val = 0;

    /* Reading input till EOS is reached.
    ** As all the output plane buffers are queued, a buffer
    ** is dequeued first before new data is read and queued back.
    */
    while (!ctx.in_error && !ctx.eos)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        Buffer *buffer =  new Buffer(ctx.outplane_buf_type, ctx.outplane_mem_type, 0);

        memset(&v4l2_buf, 0, sizeof (v4l2_buf));
        memset(planes, 0, sizeof (planes));

        v4l2_buf.m.planes = planes;

        // Dequeue the empty buffer on output plane.

        ret_val = dq_buffer(&ctx, v4l2_buf, &buffer, ctx.outplane_buf_type,
                    ctx.outplane_mem_type, 10);
        if (ret_val < 0)
        {
            cerr << "Error while DQing buffer at output plane" << endl;
            ctx.in_error = 1;
            break;
        }

        // Read and enqueue the filled buffer.

        ret_val = read_video_frame(ctx.input_file, *buffer);
        if (ret_val < 0)
        {
            cerr << "Could not read complete frame from input file" << endl;
            ctx.eos = true;
            v4l2_buf.m.planes[0].m.userptr = 0;
            v4l2_buf.m.planes[0].bytesused =
                v4l2_buf.m.planes[1].bytesused =
                v4l2_buf.m.planes[2].bytesused = 0;
        }

        if (ctx.outplane_mem_type == V4L2_MEMORY_MMAP ||
                ctx.outplane_mem_type == V4L2_MEMORY_DMABUF)
        {
            for (uint32_t j = 0; j < buffer->n_planes; ++j)
            {
                ret_val = NvBufferMemSyncForDevice(buffer->planes[j].fd, j,
                    (void **)&buffer->planes[j].data);
                if (ret_val < 0)
                {
                    cerr << "Error while NvBufferMemSyncForDevice at output plane" << endl;
                    ctx.in_error = 1;
                    break;
                }
            }
        }

        ret_val = q_buffer(&ctx,  v4l2_buf, buffer, ctx.outplane_buf_type,
            ctx.outplane_mem_type, ctx.outplane_num_planes);
        if (ret_val)
        {
            cerr << "Error while queueing buffer on output plane" << endl;
            ctx.in_error = 1;
            break;
        }

        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            cout << "File read complete." << endl;
            ctx.eos = true;
            break;
        }

    }

    return ret_val;
}

int main (int argc, char const *argv[])
{
    context_t ctx;
    int ret = 0;
    int flags = 0;
    struct v4l2_capability encoder_caps;
    struct v4l2_buffer outplane_v4l2_buf;
    struct v4l2_plane outputplanes[MAX_PLANES];
    struct v4l2_exportbuffer outplane_expbuf;
    struct v4l2_buffer capplane_v4l2_buf;
    struct v4l2_plane captureplanes[MAX_PLANES];
    struct v4l2_exportbuffer capplane_expbuf;

    // Initialisation.

    memset(&ctx, 0, sizeof (context_t));
    ctx.raw_pixfmt = V4L2_PIX_FMT_YUV420M;
    ctx.encode_pixfmt = V4L2_PIX_FMT_H264;
    ctx.outplane_mem_type = V4L2_MEMORY_MMAP;
    ctx.capplane_mem_type = V4L2_MEMORY_MMAP;
    ctx.outplane_buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    ctx.capplane_buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ctx.fd = -1;
    ctx.outplane_buffers = NULL;
    ctx.capplane_buffers = NULL;
    ctx.num_queued_outplane_buffers = 0;
    ctx.num_queued_capplane_buffers = 0;
    ctx.dqthread_running = false;
    ctx.enc_dq_thread = 0;
    pthread_mutex_init(&ctx.queue_lock, NULL);
    pthread_cond_init(&ctx.queue_cond, NULL);

    assert(argc == 5);
    ctx.input_file_path = argv[1];
    ctx.output_file_path = argv[4];
    ctx.width = atoi(argv[2]);
    ctx.height = atoi(argv[3]);

    // I/O file operations.

    ctx.input_file = new ifstream(ctx.input_file_path);
    CHECK_ERROR(!ctx.input_file->is_open(),
        "Error in opening input file", cleanup);

    ctx.output_file = new ofstream(ctx.output_file_path);
    CHECK_ERROR(!ctx.output_file->is_open(),
        "Error in opening output file", cleanup);

    /* The call creates a new V4L2 Video Encoder object
    ** on the device node "/dev/nvhost-msenc"
    ** Additional flags can also be given with which the device
    ** should be opened.
    ** This opens the device in Blocking mode.
    */

    ctx.fd = v4l2_open(ENCODER_DEV, flags | O_RDWR);
    CHECK_ERROR(ctx.fd == -1,
        "Error in opening encoder device", cleanup);

    /* The Querycap Ioctl call queries the video capabilities
    ** of the opened node and checks for
    ** V4L2_CAP_VIDEO_M2M_MPLANE capability on the device.
    */

    ret = v4l2_ioctl(ctx.fd, VIDIOC_QUERYCAP, &encoder_caps);
    CHECK_ERROR(ret, "Failed to query video capabilities", cleanup);

    if (!(encoder_caps.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE))
    {
        cerr << "Device does not support V4L2_CAP_VIDEO_M2M_MPLANE" << endl;
        ctx.in_error = 1;
        goto cleanup;
    }

    /* It is necessary to set capture plane
    ** format before the output plane format
    ** along with the frame width and height.
    ** The format of the encoded bitstream is set.
    */

    ret = set_capture_plane_format(ctx, 2 * 1024 * 1024);
    CHECK_ERROR(ret, "Error in setting capture plane format", cleanup);

    // Set format on output plane.

    ret = set_output_plane_format(ctx);
    CHECK_ERROR(ret, "Error in setting output plane format", cleanup);

    /* The H264 properties and streaming
    ** parameters are set default by the encoder.
    ** They can be modified by calling
    ** VIDIOC_S_PARAM and VIDIOC_S_EXT_CTRLS.
    */

    /* Request buffers on output plane to fill
    ** the raw data.
    */

    ret = req_buffers_on_output_plane(&ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        ctx.outplane_mem_type, 10);
    CHECK_ERROR(ret, "Error in requesting buffers on output plane", cleanup);

    /* Query the status of requested buffers
    ** For each requested buffer, export buffer
    ** and map it for MMAP memory.
    */

    for (uint32_t i = 0; i < ctx.outplane_num_buffers; ++i)
    {
        memset(&outplane_v4l2_buf, 0, sizeof (struct v4l2_buffer));
        memset(outputplanes, 0, sizeof (struct v4l2_plane));
        outplane_v4l2_buf.index = i;
        outplane_v4l2_buf.type = ctx.outplane_buf_type;
        outplane_v4l2_buf.memory = ctx.outplane_mem_type;
        outplane_v4l2_buf.m.planes = outputplanes;
        outplane_v4l2_buf.length = ctx.outplane_num_planes;

        ret = v4l2_ioctl(ctx.fd, VIDIOC_QUERYBUF, &outplane_v4l2_buf);
        CHECK_ERROR(ret, "Error in querying for "<< i <<
            "th buffer outputplane", cleanup);

        for (uint32_t j = 0; j < outplane_v4l2_buf.length; ++j)
        {
            ctx.outplane_buffers[i]->planes[j].length =
                outplane_v4l2_buf.m.planes[j].length;
            ctx.outplane_buffers[i]->planes[j].mem_offset =
                outplane_v4l2_buf.m.planes[j].m.mem_offset;
        }

        memset(&outplane_expbuf, 0, sizeof (struct v4l2_exportbuffer));
        outplane_expbuf.type = ctx.outplane_buf_type;
        outplane_expbuf.index = i;

        for (uint32_t j = 0; j < ctx.outplane_num_planes; ++j)
        {
            outplane_expbuf.plane = j;
            ret = v4l2_ioctl(ctx.fd, VIDIOC_EXPBUF, &outplane_expbuf);
            CHECK_ERROR(ret, "Error in exporting "<< i <<
                "th index buffer outputplane", cleanup);

            ctx.outplane_buffers[i]->planes[j].fd = outplane_expbuf.fd;
        }

        if (ctx.outplane_buffers[i]->map())
        {
            cerr << "Buffer mapping error on output plane" << endl;
            ctx.in_error = 1;
            goto cleanup;
        }

    }

    // Request buffers on capture plane.

    ret = req_buffers_on_capture_plane(&ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
        ctx.capplane_mem_type, 6);
    CHECK_ERROR(ret, "Error in requesting buffers on capture plane", cleanup);

    /* Query the status of requested buffers
    ** For each requested buffer, export buffer
    ** and map it for MMAP memory.
    */

    for (uint32_t i = 0; i < ctx.capplane_num_buffers; ++i)
    {
        memset(&capplane_v4l2_buf, 0, sizeof (struct v4l2_buffer));
        memset(captureplanes, 0, sizeof (struct v4l2_plane));
        capplane_v4l2_buf.index = i;
        capplane_v4l2_buf.type = ctx.capplane_buf_type;
        capplane_v4l2_buf.memory = ctx.capplane_mem_type;
        capplane_v4l2_buf.m.planes = captureplanes;
        capplane_v4l2_buf.length = ctx.capplane_num_planes;

        ret = v4l2_ioctl(ctx.fd, VIDIOC_QUERYBUF, &capplane_v4l2_buf);
        CHECK_ERROR(ret, "Error in querying for "<< i <<
            "th buffer captureplane", cleanup);

        for (uint32_t j = 0; j < capplane_v4l2_buf.length; ++j)
        {
            ctx.capplane_buffers[i]->planes[j].length =
                capplane_v4l2_buf.m.planes[j].length;
            ctx.capplane_buffers[i]->planes[j].mem_offset =
                capplane_v4l2_buf.m.planes[j].m.mem_offset;
        }

        memset(&capplane_expbuf, 0, sizeof (struct v4l2_exportbuffer));
        capplane_expbuf.type = ctx.capplane_buf_type;
        capplane_expbuf.index = i;

        for (uint32_t j = 0; j < ctx.capplane_num_planes; ++j)
        {
            capplane_expbuf.plane = j;
            ret = v4l2_ioctl(ctx.fd, VIDIOC_EXPBUF, &capplane_expbuf);
            CHECK_ERROR(ret, "Error in exporting "<< i <<
                "th index buffer captureplane", cleanup);

            ctx.capplane_buffers[i]->planes[j].fd = capplane_expbuf.fd;
        }

        if (ctx.capplane_buffers[i]->map())
        {
            cerr << "Buffer mapping error on capture plane" << endl;
            ctx.in_error = 1;
            goto cleanup;
        }
    }

    /* Subscribe to EOS event, triggered
    ** when zero sized buffer is enquequed
    ** on output plane.
    */

    ret = subscribe_event(ctx.fd, V4L2_EVENT_EOS, 0, 0);
    CHECK_ERROR(ret, "Error in subscribing to EOS change", cleanup);

    /* Set streaming on both plane
    ** Start stream processing on output plane and capture
    ** plane by setting the streaming status ON.
    */

    ret = v4l2_ioctl(ctx.fd, VIDIOC_STREAMON, &ctx.outplane_buf_type);
    CHECK_ERROR(ret, "Error in setting streaming status ON output plane", cleanup);

    ctx.outplane_streamon = 1;

    ret = v4l2_ioctl (ctx.fd, VIDIOC_STREAMON, &ctx.capplane_buf_type);
    CHECK_ERROR(ret, "Error in setting streaming status ON capture plane", cleanup);

    ctx.capplane_streamon = 1;

    /* Create DQ Capture loop thread
    ** and set the callback function to dq_thread.
    */

    pthread_mutex_lock(&ctx.queue_lock);
    pthread_create(&ctx.enc_dq_thread, NULL, dq_thread, &ctx);
    ctx.dqthread_running = true;
    pthread_mutex_unlock(&ctx.queue_lock);

    // First enqueue all the empty buffers on capture plane.

    for (uint32_t i = 0; i < ctx.capplane_num_buffers; ++i)
    {
        struct v4l2_buffer queue_cap_v4l2_buf;
        struct v4l2_plane queue_cap_planes[MAX_PLANES];
        Buffer *buffer;

        memset(&queue_cap_v4l2_buf, 0, sizeof (struct v4l2_buffer));
        memset(queue_cap_planes, 0, MAX_PLANES * sizeof (struct v4l2_plane));

        buffer = ctx.capplane_buffers[i];
        queue_cap_v4l2_buf.index = i;
        queue_cap_v4l2_buf.m.planes = queue_cap_planes;

        ret = q_buffer(&ctx, queue_cap_v4l2_buf, buffer, ctx.capplane_buf_type,
                ctx.capplane_mem_type, ctx.capplane_num_planes);
        CHECK_ERROR(ret, "Error while queueing buffer on capture plane", cleanup);
    }

    /* Now read the raw data and enqueue buffers
    ** on output plane. Exit loop in case file read is complete.
    */

    for (uint32_t i = 0; i < ctx.outplane_num_buffers; ++i)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        Buffer *buffer;

        memset(&v4l2_buf, 0, sizeof (v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof (struct v4l2_plane));

        buffer = ctx.outplane_buffers[i];
        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        ret = read_video_frame(ctx.input_file, *buffer);
        if (ret < 0)
        {
            cerr << "Could not read complete frame from input file" << endl;
            ctx.eos = true;
            v4l2_buf.m.planes[0].m.userptr = 0;
            v4l2_buf.m.planes[0].bytesused =
                v4l2_buf.m.planes[1].bytesused =
                v4l2_buf.m.planes[2].bytesused = 0;
        }

        if (ctx.outplane_mem_type == V4L2_MEMORY_MMAP ||
                ctx.outplane_mem_type == V4L2_MEMORY_DMABUF)
        {
            for (uint32_t j = 0; j < buffer->n_planes; ++j)
            {
                ret = NvBufferMemSyncForDevice(buffer->planes[j].fd, j,
                    (void **)&buffer->planes[j].data);
                CHECK_ERROR(ret < 0,
                    "Error while NvBufferMemSyncForDevice at output plane", cleanup);
            }
        }

        /* Enqueue the buffer on output plane
        ** It is necessary to queue an empty buffer
        ** to signal EOS to the encoder.
        */
        ret = q_buffer(&ctx, v4l2_buf, buffer, ctx.outplane_buf_type,
            ctx.outplane_mem_type, ctx.outplane_num_planes);
        CHECK_ERROR(ret, "Error while queueing buffer on output plane", cleanup);

        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            cout << "File read complete." << endl;
            ctx.eos = true;
            break;
        }
    }

    // Dequeue and queue loop on output plane.

    ret = encoder_process_blocking(ctx);
    CHECK_ERROR(ret < 0, "Encoder is in error", cleanup);

    /* For blocking mode, after getting EOS on output plane, wait
    ** till all the buffers are successfully from the capture plane.
    */
    wait_for_dqthread(ctx, -1);

    // Cleanup and exit.

cleanup:
    if (ctx.fd != -1)
    {

        // Stream off on both planes.

        ret = v4l2_ioctl(ctx.fd, VIDIOC_STREAMOFF, &ctx.outplane_buf_type);
        ctx.outplane_streamon = 0;
        ret = v4l2_ioctl(ctx.fd, VIDIOC_STREAMOFF, &ctx.capplane_buf_type);
        ctx.capplane_streamon = 0;

        // Unmap MMAPed buffers.

        for (uint32_t i = 0; i < ctx.outplane_num_buffers; ++i)
        {
            ctx.outplane_buffers[i]->unmap();
        }
        for (uint32_t i = 0; i < ctx.capplane_num_buffers; ++i)
        {
            ctx.capplane_buffers[i]->unmap();
        }

        // Request 0 buffers on both planes.

        ret = req_buffers_on_output_plane(&ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
            ctx.outplane_mem_type, 0);
        ret = req_buffers_on_capture_plane(&ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
            ctx.capplane_mem_type, 0);

        // Close the opened V4L2 device.

        ret = v4l2_close(ctx.fd);
        if (ret)
        {
            cerr << "Unable to close the device" << endl;
            ctx.in_error = 1;
        }

    }

    ctx.input_file->close();
    ctx.output_file->close();

    delete ctx.input_file;
    delete ctx.output_file;

    if (ctx.in_error)
    {
        cerr << "Encoder is in error << endl" << endl;
    }

    else
    {
        cout << "Encoder Run Successful" << endl;
    }

    return ret;
}