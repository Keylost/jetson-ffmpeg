/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
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
 * ./camera_sample -r (int)width (int)height
 * Eg: ./camera_sample -r 1920 1080
 **/

#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fstream>
#include <signal.h>
#include <libv4l2.h>
#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

#include "nvbuf_utils.h"
#include "v4l2_nv_extensions.h"

using namespace std;

#include "camera_unit_sample.hpp"
#include "camera_unit_sample_ctrls.hpp"

/**
 *
 * V4L2 Video Camera Sample
 *
 * The default video camera device node is
 *     /dev/video0
 *
 * In this sample:
 * ## Pixel Formats
 * CAPTURE PLANE
 * :----------------:
 * V4L2_PIX_FMT_NV12M
 *
 * ## Memory Type
 *            | CAPTURE PLANE
 * :--------: | :-----------:
 * MEMORY     | V4L2_MEMORY_MMAP
 *            | V4L2_MEMORY_DMABUF
 *
 * ## Supported Controls
 * Refer v4l2_nv_extensions.h for control information
 * - #V4L2_CID_ARGUS_AUTO_WHITE_BALANCE_MODE
 * - #V4L2_CID_ARGUS_SENSOR_MODE
 * - #V4L2_CID_ARGUS_DENOISE_STRENGTH
 * - #V4L2_CID_ARGUS_DENOISE_MODE
 * - #V4L2_CID_ARGUS_EE_STRENGTH
 * - #V4L2_CID_ARGUS_EE_MODE
 * - #V4L2_CID_ARGUS_AE_ANTIBANDING_MODE
 * - #V4L2_CID_ARGUS_ISP_DIGITAL_GAIN_RANGE
 * - #V4L2_CID_ARGUS_COLOR_SATURATION
 * - #V4L2_CID_ARGUS_GAIN_RANGE
 * - #V4L2_CID_ARGUS_EXPOSURE_TIME_RANGE
 *
 * ## Opening the Camera
 * The camera device node is opened through the v4l2_open IOCTL call.
 * After opening the device, the application calls VIDIOC_QUERYCAP to identify
 * the driver capabilities.
 *
 * ## Setting up the capture plane
 * The application subscribes to the V4L2_EVENT_EOS event,
 * to detect the end of stream and handle the plane buffers
 * accordingly.
 * The application calls VIDIOC_S_FMT to setup the format required on
 * CAPTURE PLANE for the driver.
 * It is necessary to set capture plane format before setting any controls.
 *
 * ## Setting Controls
 * The application gets/sets the properties of the encoder by setting
 * the controls, calling VIDIOC_S_EXT_CTRLS, VIDIOC_G_EXT_CTRLS.
 *
 * * ### Setting Framerate
 * The camera framerate is capped by the camera mode set.
 * If the camera mode is to be selected along with the specified framerate,
 * then camera mode must be selected before setting the framerate.
 *
 * ## Buffer Management
 * Buffers are requested on the CAPTURE PLANE by the application, calling
 * VIDIOC_REQBUFS. The actual buffers allocated by the encoder are then
 * queried and exported as FD for the MMAPed buffers.
 * For each DMA allocated buffers, the application must also call the
 * VIDIOC_QUERYBUF.
 * Status STREAMON is called on the capture plane to signal for
 * processing.
 *
 * Application continuously queues the empty buffers to the camera driver.
 * The camera fills the buffer and signals a successful dequeue
 * on the capture plane, from where the data of v4l2_buffer dequeued is either
 * rendered or dumped as raw frames or both.
 *
 * The capture thread blocks on the DQ buffer call, which returns either after
 * a successful DQ or after a specific timeout.
 *
 * ## EOS Handling
 * For sending EOS to the camera, the application should
 * - Stop queueing empty buffers on capture plane.
 * - Dequeue buffers on the capture plane until it gets a buffer with bytesused = 0
 *
 * After the last buffer on the capture plane is dequeued, set STREAMOFF
 * and destroy the allocated buffers.
 *
 */

#define CHECK_ERROR(condition, error_str, label) if (condition) { \
                                                        cerr << error_str << endl; \
                                                        ctx.in_error = 1; \
                                                        goto label; }

static bool quit_capture = false;

void set_defaults (context_t *ctx);

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

void
set_defaults (context_t *ctx)
{
    memset(ctx, 0, sizeof (context_t));
    memset(&ctx->ctrls, 0, sizeof (argus_controls));
    memset(&ctx->display, 0, sizeof(display_settings));
    memset(&ctx->capplane, 0, sizeof(capture_plane));

    ctx->raw_pixfmt = V4L2_PIX_FMT_NV12M;
    ctx->capplane.mem_type = V4L2_MEMORY_MMAP;
    ctx->capplane.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ctx->fd = -1;
    ctx->stress_test = 1;
    ctx->capplane.buffers = NULL;
    ctx->capplane.num_queued_buffers = 0;
    ctx->dqthread_running = false;
    ctx->cam_dq_thread = 0;
    ctx->num_frames = DEFAULT_NUM_FRAMES;
    ctx->width = 1280;
    ctx->height = 720;

    ctx->ctrls.fps_n = DEFAULT_ARGUS_FPS;
    ctx->ctrls.fps_d = 1;
    ctx->display.window_width = 0;
    ctx->display.window_height = 0;
    ctx->display.window_xoff = 0;
    ctx->display.window_yoff = 0;
    ctx->fullscreen_mode = false;
    ctx->disable_rendering = false;
    ctx->ctrls.sensor_id = DEFAULT_ARGUS_SENSOR_ID;
    ctx->ctrls.sensor_mode = DEFAULT_ARGUS_SENSOR_MODE;
    ctx->set_awbmode = false;
    ctx->set_denoisemode = false;
    ctx->set_eemode = false;
    ctx->set_aeantibandingmode = false;
    ctx->set_denoise_strength = false;
    ctx->set_eestrength = false;
    ctx->set_autolock = false;
    ctx->set_expcompensation = false;
    ctx->set_ispdigitalgainrange = false;
    ctx->set_gainrange = false;
    ctx->set_exptimerange = false;
    ctx->set_colorsaturation = false;
    ctx->enable_metadata = false;

    ctx->ctrls.autowhitebalance_mode = DEFAULT_ARGUS_AWB_MODE;
    ctx->ctrls.denoise_mode = DEFAULT_ARGUS_DENOISE_MODE;
    ctx->ctrls.denoise_strength = DEFAULT_ARGUS_DENOISE_STRENGTH;
    ctx->ctrls.edge_enhacement_mode = DEFAULT_ARGUS_EE_MODE;
    ctx->ctrls.ae_antibanding_mode = DEFAULT_ARGUS_AEANTIBANDING_MODE;
    ctx->ctrls.edge_enhacement_strength = DEFAULT_ARGUS_EE_STRENGTH;
    ctx->ctrls.aelock = DEFAULT_ARGUS_AE_LOCK;
    ctx->ctrls.awblock = DEFAULT_ARGUS_AWB_LOCK;
    ctx->ctrls.exposure_compensation = DEFAULT_ARGUS_EXP_COMPENSATION;
    ctx->ctrls.isp_digital_gain_range = {DEFAULT_ARGUS_DIGITAL_GAIN_RANGE_MIN,
        DEFAULT_ARGUS_DIGITAL_GAIN_RANGE_MAX};
    ctx->ctrls.gain_range = {DEFAULT_ARGUS_GAIN_RANGE_MIN,
        DEFAULT_ARGUS_GAIN_RANGE_MAX};
    ctx->ctrls.exposure_time_range = {static_cast<float>(DEFAULT_ARGUS_EXPOSURE_TIME_MIN),
        static_cast<float>(DEFAULT_ARGUS_EXPOSURE_TIME_MAX)};
    ctx->ctrls.color_saturation = DEFAULT_ARGUS_DIGITAL_SATURATION;

    ctx->capplane.format_set = false;
    pthread_mutex_init(&ctx->queue_lock, NULL);
    pthread_cond_init(&ctx->queue_cond, NULL);
}

static void
signal_handle(int signum)
{
    cerr << "Interrupt signal. Quitting\n" << endl;
    quit_capture = true;
}

int
write_frame(std::ofstream * stream, Buffer *buffer)
{
    uint32_t i = 0, j;
    char *data;

    for (i = 0; i < buffer->n_planes; i++)
    {
        Buffer::BufferPlane &plane = buffer->planes[i];
        size_t bytes_to_write =
            plane.fmt.bytesperpixel * plane.fmt.width;

        data = (char *) plane.data;
        for (j = 0; j < plane.fmt.height; j++)
        {
            stream->write(data, bytes_to_write);
            if (!stream->good())
                return -1;
            data += plane.fmt.stride;
        }
    }
    return 0;
}

int
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
        pthread_join(ctx.cam_dq_thread, NULL);
        ctx.cam_dq_thread = 0;
    }
    else
    {
        cerr << "Time out waiting for dqthread" << endl;
        ctx.in_error = 1;
    }
    return ret_val;
}

int
set_plane_format(context_t& ctx)
{
    struct v4l2_format format;
    int ret_val = 0;
    uint32_t num_bufferplanes;
    Buffer::BufferPlaneFormat planefmts[MAX_PLANES];

    if (ctx.raw_pixfmt != V4L2_PIX_FMT_NV12M)
    {
        cerr << "Only V4L2_PIX_FMT_NV12M is supported" << endl;
        return -1;
    }

    Buffer::fill_buffer_plane_format(&num_bufferplanes, planefmts, ctx.width,
            ctx.height, ctx.raw_pixfmt);

    ctx.capplane.num_planes = num_bufferplanes;
    for (uint32_t i = 0; i < num_bufferplanes; ++i)
    {
        ctx.capplane.planefmts[i] = planefmts[i];
    }
    memset(&format, 0, sizeof (struct v4l2_format));
    format.type = ctx.capplane.buf_type;
    format.fmt.pix_mp.width = ctx.width;
    format.fmt.pix_mp.height = ctx.height;
    format.fmt.pix_mp.pixelformat = ctx.raw_pixfmt;
    format.fmt.pix_mp.num_planes = num_bufferplanes;

    ret_val = v4l2_ioctl(ctx.fd, VIDIOC_S_FMT, &format);
    if (!ret_val)
    {
        ctx.capplane.num_planes = format.fmt.pix_mp.num_planes;
        for (uint32_t j = 0; j < ctx.capplane.num_planes; j++)
        {
            ctx.capplane.planefmts[j].stride =
                format.fmt.pix_mp.plane_fmt[j].bytesperline;
            ctx.capplane.planefmts[j].sizeimage =
                format.fmt.pix_mp.plane_fmt[j].sizeimage;
        }
    }
    ctx.capplane.format_set = true;
    return ret_val;
}

int
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
        ctx->capplane.buffers = new Buffer *[reqbuffers.count];
        for (uint32_t i = 0; i < reqbuffers.count; ++i)
        {
            ctx->capplane.buffers[i] = new Buffer (buf_type, mem_type,
                ctx->capplane.num_planes, ctx->capplane.planefmts, i);
        }
    }
    else
    {
        for (uint32_t i = 0; i < ctx->capplane.num_buffers; ++i)
        {
            delete ctx->capplane.buffers[i];
        }
        delete[] ctx->capplane.buffers;
        ctx->capplane.buffers = NULL;
    }
    ctx->capplane.num_buffers = reqbuffers.count;

    return ret_val;
}

int
allocate_dmabuffers(context_t * ctx, enum v4l2_buf_type buf_type,
        enum v4l2_memory mem_type, int num_buffers)
{
    int ret_val = 0;
    int fd;
    NvBufferCreateParams cParams;
    struct v4l2_requestbuffers reqbuffers;

    memset (&reqbuffers, 0, sizeof (struct v4l2_requestbuffers));

    reqbuffers.count = num_buffers;
    reqbuffers.memory = mem_type;
    reqbuffers.type = buf_type;

    ret_val = v4l2_ioctl (ctx->fd, VIDIOC_REQBUFS, &reqbuffers);
    if (ret_val)
    {
        cerr << "REQBUFs IOCTL failed for V4L2_MEMORY_DMABUF" << endl;
        return ret_val;
    }

    if (reqbuffers.count)
    {
        for (uint32_t i = 0; i < reqbuffers.count; i++)
        {
            cParams.width = ctx->width;
            cParams.height = ctx->height;
            cParams.layout = NvBufferLayout_Pitch;
            if (ctx->raw_pixfmt == V4L2_PIX_FMT_YUV420M)
                cParams.colorFormat = NvBufferColorFormat_YUV420;
            else
                cParams.colorFormat = NvBufferColorFormat_NV12;
            cParams.nvbuf_tag = NvBufferTag_CAMERA;
            cParams.payloadType = NvBufferPayload_SurfArray;
            ret_val = NvBufferCreateEx(&fd, &cParams);
            if(ret_val < 0)
            {
                cerr << "Failed to create NvBuffer" << endl;
                return ret_val;
            }
            ctx->dmabuffers_fd[i] = fd;
        }
    }
    else
    {
        for (uint32_t i = 0; i < MAX_CAPTURE_BUFFFERS; i++)
        {
            if (ctx->dmabuffers_fd[i])
            {
                NvBufferDestroy(ctx->dmabuffers_fd[i]);
                ctx->dmabuffers_fd[i] = 0;
            }
        }
    }

    ctx->capplane.num_buffers = reqbuffers.count;
    return ret_val;
}

int
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
            case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
                ctx->capplane.num_queued_buffers++;
                break;
            default:
                cerr << "Buffer Type not supported" << endl;
        }
        pthread_cond_broadcast (&ctx->queue_cond);
    }
    pthread_mutex_unlock (&ctx->queue_lock);

    return ret_val;
}

int
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
            if (ctx->enable_metadata)
                get_metadata(ctx->fd, v4l2_buf.index);

            pthread_mutex_lock(&ctx->queue_lock);
            switch(v4l2_buf.memory)
            {
                case V4L2_MEMORY_MMAP:
                    if (buffer)
                        *buffer = ctx->capplane.buffers[v4l2_buf.index];
                    for (uint32_t j = 0; j < ctx->capplane.buffers[v4l2_buf.index]->n_planes; j++)
                    {
                        ctx->capplane.buffers[v4l2_buf.index]->planes[j].bytesused =
                        v4l2_buf.m.planes[j].bytesused;
                    }
                    ctx->capplane.num_queued_buffers--;
                    break;
                case V4L2_MEMORY_DMABUF:
                    ctx->capplane.num_queued_buffers--;
                    break;
                default:
                    cout << "Invaild memory type" << endl;
            }
            pthread_cond_broadcast(&ctx->queue_cond);
            pthread_mutex_unlock(&ctx->queue_lock);
        }
        else if (errno == EAGAIN)
        {
            pthread_mutex_lock(&ctx->queue_lock);
            if (!ctx->capplane.streamon)
            {
                pthread_mutex_unlock(&ctx->queue_lock);
                is_in_error = true;
                break;
            }
            pthread_mutex_unlock(&ctx->queue_lock);

            if (num_retries-- == 0)
            {
                /* Resource temporarily unavailable. */
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

void *
dq_thread(void *arg)
{
    context_t *ctx = (context_t *)arg;
    ctx->dq_buffer_count = ctx->num_frames;

    while (ctx->dqthread_running && !quit_capture)
    {
        bool ret_val;
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        Buffer *buffer = NULL;

        if (ctx->dq_buffer_count == 0)
            break;

        if (ctx->capplane.mem_type != V4L2_MEMORY_DMABUF)
        {
            buffer = new Buffer(ctx->capplane.buf_type,
                ctx->capplane.mem_type, 0);
        }

        memset(&v4l2_buf, 0, sizeof (struct v4l2_buffer));
        memset(planes, 0, MAX_PLANES * sizeof (struct v4l2_plane));
        v4l2_buf.m.planes = planes;
        v4l2_buf.length = ctx->capplane.num_planes;

        if (dq_buffer(ctx, v4l2_buf, &buffer, ctx->capplane.buf_type,
                ctx->capplane.mem_type, -1) < 0)
        {
            if (errno != EAGAIN)
            {
                ctx->in_error = true;
                ret_val = capture_plane_callback(NULL, NULL, ctx);
            }

            if (errno == EAGAIN || ctx->capplane.streamon)
                ret_val = capture_plane_callback(NULL, NULL, ctx);

            if (!ctx->capplane.streamon)
            {
                break;
            }
        }
        else
        {
            ret_val = capture_plane_callback(&v4l2_buf, buffer, ctx);
            ctx->dq_buffer_count--;
        }
        if (!ret_val)
        {
            break;
        }
    }

    pthread_mutex_lock(&ctx->queue_lock);
    ctx->dqthread_running = false;
    pthread_cond_broadcast(&ctx->queue_cond);
    pthread_mutex_unlock(&ctx->queue_lock);

    return NULL;
}

int dump_dmabuffers(ofstream *stream, int dmabuf_fd)
{
    int ret = 0;
    int num_planes = 0;
    NvBufferParams parm;

    if (dmabuf_fd <= 0)
        return -1;

    ret = NvBufferGetParams(dmabuf_fd, &parm);
    if (ret != 0)
    {
        cerr << "Get NvBufferParams failed \n" << endl;
        return -1;
    }
    if (parm.pixel_format == NvBufferColorFormat_NV12)
        num_planes = 2;
    else
    {
        cout << "Currently NV12 only supported" << endl;
        return -1;
    }
    for (int plane = 0; plane < num_planes; plane++)
    {
        void *psrc_data;
        ret = NvBufferMemMap(dmabuf_fd, plane, NvBufferMem_Read_Write, &psrc_data);
        if (ret == 0)
        {
            unsigned int i = 0;
            NvBufferMemSyncForCpu(dmabuf_fd, plane, &psrc_data);
            for (i = 0; i < parm.height[plane]; ++i)
            {
                if((parm.pixel_format == NvBufferColorFormat_NV12) && plane == 1)
                {
                    stream->write((char *)psrc_data + i * parm.pitch[plane],
                                    parm.width[plane] * 2);
                    if (!stream->good())
                        return -1;
                }
                else
                {
                    stream->write((char *)psrc_data + i * parm.pitch[plane],
                                    parm.width[plane]);
                    if (!stream->good())
                        return -1;
                }
            }
            NvBufferMemUnMap(dmabuf_fd, plane, &psrc_data);
        }
    }

    return 0;
}

bool
capture_plane_callback(struct v4l2_buffer *v4l2_buf, Buffer * buffer, void *arg)
{
    context_t *ctx = (context_t *)arg;

    if (v4l2_buf == NULL)
    {
        cout << "Error while DQing buffer from capture plane" << endl;
        ctx->in_error = 1;
        return false;
    }

    if (buffer != NULL && (buffer->planes[0].bytesused == 0))
    {
        cout << "Got 0 size buffer in capture" << endl;
        return false;
    }

    if (ctx->output_file)
    {
        if (ctx->capplane.mem_type == V4L2_MEMORY_DMABUF)
        {
            dump_dmabuffers(ctx->output_file, v4l2_buf->m.planes[0].m.fd);
        }
        else
        {
            write_frame(ctx->output_file, buffer);
        }
    }

    if (!ctx->disable_rendering)
    {
        if (ctx->capplane.mem_type == V4L2_MEMORY_DMABUF)
        {
            ctx->display.renderer->render(v4l2_buf->m.planes[0].m.fd);
        }
        else
            ctx->display.renderer->render(buffer->planes[0].fd);
    }

    if (q_buffer(ctx, *v4l2_buf, buffer, ctx->capplane.buf_type, ctx->capplane.mem_type,
            ctx->capplane.num_planes) < 0)
    {
        cerr << "Error while Qing buffer at capture plane" <<  endl;
        ctx->in_error = 1;
        return false;
    }

    return true;
}

int set_controls(context_t *ctx)
{

    if (ctx->capplane.format_set)
    {
        if (ctx->ctrls.sensor_mode != DEFAULT_ARGUS_SENSOR_MODE)
        {
            if (set_sensor_mode(ctx->fd, ctx->ctrls.sensor_mode))
            {
                cout << "S_EXT_CTRLS for Sensor mode failed\n";
                return -1;
            }
        }
        if (ctx->set_awbmode)
        {
            if (set_awb_mode(ctx->fd, ctx->ctrls.autowhitebalance_mode))
            {
                cout << "S_EXT_CTRLS for AWB mode failed\n";
                return -1;
            }
        }
        if (ctx->set_denoisemode)
        {
            if (set_denoise_mode(ctx->fd, ctx->ctrls.denoise_mode))
            {
                cout << "S_EXT_CTRLS for TNR mode failed\n";
                return -1;
            }
        }
        if (ctx->set_denoise_strength)
        {
            if (set_denoise_strength(ctx->fd, ctx->ctrls.denoise_strength))
            {
                cout << "S_EXT_CTRLS for TNR strength failed\n";
                return -1;
            }
        }
        if (ctx->set_eemode)
        {
            if (set_ee_mode(ctx->fd, ctx->ctrls.edge_enhacement_mode))
            {
                cout << "S_EXT_CTRLS for Edge Enhancement mode failed\n";
                return -1;
            }
        }
        if (ctx->set_eestrength)
        {
            if (set_ee_strength(ctx->fd, ctx->ctrls.edge_enhacement_strength))
            {
                cout << "S_EXT_CTRLS for Edge Enhancement strength failed\n";
                return -1;
            }
        }
        if (ctx->set_aeantibandingmode)
        {
            if (set_ae_antiband_mode(ctx->fd, ctx->ctrls.ae_antibanding_mode))
            {
                cout << "S_EXT_CTRLS for AE Antibanding mode failed\n";
                return -1;
            }
        }
        if (ctx->set_autolock)
        {
            if (set_autolock(ctx->fd, ctx->ctrls.aelock, ctx->ctrls.awblock))
            {
                cout << ("S_EXT_CTRLS for AE and AWB locks failed\n");
                return -1;
            }
        }
        if (ctx->set_expcompensation)
        {
            if (set_exposure_compensation(ctx->fd, ctx->ctrls.exposure_compensation))
            {
                cout << ("S_EXT_CTRLS for Exposure Compensation failed\n");
                return -1;
            }
        }
        if (ctx->set_ispdigitalgainrange)
        {
            if (set_digital_gain_range(ctx->fd, ctx->ctrls.isp_digital_gain_range.low,
                ctx->ctrls.isp_digital_gain_range.high))
            {
                cout << "S_EXT_CTRLS for ISP Digital Gain failed\n";
                return -1;
            }
        }
        if (ctx->set_colorsaturation)
        {
            if (set_color_saturation(ctx->fd, ctx->ctrls.color_saturation))
            {
                cout << ("S_EXT_CTRLS for Color Saturation failed\n");
                return -1;
            }
        }
        if (ctx->set_gainrange)
        {
            if (set_gain_range(ctx->fd, ctx->ctrls.gain_range.low,
                ctx->ctrls.gain_range.high))
            {
                cout << "S_EXT_CTRLS for Gain Range failed\n";
                return -1;
            }
        }
        if (ctx->set_exptimerange)
        {
            if (set_exp_time_range(ctx->fd, ctx->ctrls.exposure_time_range.low,
                ctx->ctrls.exposure_time_range.high))
            {
                cout << "S_EXT_CTRLS for Exposure Time Range failed\n";
                return -1;
            }
        }

    }
    return 0;
}

int capture_proc (context_t &ctx, int argc, char const *argv[])
{
    int32_t ret = 0;
    int32_t flags = 0;
    int32_t window_width = 0;
    int32_t window_height = 0;
    struct v4l2_capability camera_caps;
    struct v4l2_buffer capplane_v4l2_buf;
    struct v4l2_plane captureplanes[MAX_PLANES];
    struct v4l2_exportbuffer capplane_expbuf;
    struct sigaction sig_action;
    char camera_device[16];
    char renderer[16];

    /* Register a shutdown handler to ensure
    ** a clean exit if <ctrl+c> is detected.
    */
    sig_action.sa_handler = signal_handle;
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags = 0;
    sigaction(SIGINT, &sig_action, NULL);

    /* Initialization. */
    set_defaults(&ctx);
    ret = parse_cmdline_args(&ctx, argc, argv);
    CHECK_ERROR(ret < 0, "Error in parsing commandline args", cleanup);

    /* Open the output file for dump. */
    if (ctx.output_file_path)
    {
        ctx.output_file = new ofstream(ctx.output_file_path);
        CHECK_ERROR(!ctx.output_file->is_open(),
            "Error in opening output file", cleanup);
    }

    if (!ctx.disable_rendering)
    {
        /* Destroy the old instance of renderer. */
        if (ctx.display.renderer)
        {
            delete ctx.display.renderer;
            ctx.display.renderer = NULL;
        }

        if (ctx.fullscreen_mode)
        {
            /* Required for fullscreen. */
            ctx.display.window_width = 0;
            ctx.display.window_height = 0;
        }
        else if (ctx.display.window_width && ctx.display.window_height)
        {
            window_width = ctx.display.window_width;
            window_height = ctx.display.window_height;
        }
        else
        {
            window_width = ctx.width;
            window_height = ctx.height;
        }

        /* If height or width are set to zero, EglRenderer creates a fullscreen
           window for rendering. */
        snprintf(renderer, sizeof (renderer), "renderer%d", ctx.ctrls.sensor_id);
        ctx.display.renderer = NvEglRenderer::createEglRenderer(renderer,
                        window_width, window_height, ctx.display.window_xoff,
                        ctx.display.window_yoff);

        CHECK_ERROR(!ctx.display.renderer, "Error in setting up EglRenderer."
                "Check if X is running or run with -nr", cleanup);
        if (ctx.display.renderer)
        {
            ctx.display.renderer->setFPS((int32_t)(ctx.ctrls.fps_n/ctx.ctrls.fps_d));
        }
    }

    /* The call creates a new V4L2 Video Camera object
    ** on the device node "/dev/videox"
    ** Additional flags can also be given with which the device
    ** should be opened.
    ** This opens the device in Blocking mode.
    */
    snprintf(camera_device, sizeof (camera_device), "%s%d",
            CAMERA_DEV, ctx.ctrls.sensor_id);

    cout << "Sensor ID " << ctx.ctrls.sensor_id << endl;

    ctx.fd = v4l2_open(camera_device, flags | O_RDWR);
    CHECK_ERROR(ctx.fd == -1, "Error in opening camera device", cleanup);

    /* The Querycap Ioctl call queries the video capabilities
    ** of the opened node and checks for
    ** V4L2_CAP_VIDEO_CAPTURE_MPLANE capability on the device.
    */

    ret = v4l2_ioctl(ctx.fd, VIDIOC_QUERYCAP, &camera_caps);
    CHECK_ERROR(ret, "Failed to query video capabilities", cleanup);

    if (!(camera_caps.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE))
    {
        cerr << "Device does not support V4L2_CAP_VIDEO_CAPTURE_MPLANE" << endl;
        ctx.in_error = 1;
        goto cleanup;
    }

    /* Set format on capture plane. */

    ret = set_plane_format(ctx);
    CHECK_ERROR(ret, "Error in setting output plane format", cleanup);

    /* Set framerate and controls */

    if (ctx.capplane.format_set)
    {
        ret = set_controls(&ctx);
        CHECK_ERROR(ret, "Error in setting control", cleanup);

        if (set_framerate(ctx.fd, ctx.ctrls.fps_n, ctx.ctrls.fps_d))
            cout << "Failed to set framerate. Default set by the library\n";
        else
            cout << "Framerate set to " << ctx.ctrls.fps_n << "/"
                << ctx.ctrls.fps_d << endl;
    }

    /* Request buffers on capture plane. */

    if (ctx.capplane.mem_type == V4L2_MEMORY_DMABUF)
    {
        ret = allocate_dmabuffers(&ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
                ctx.capplane.mem_type, 10);
        CHECK_ERROR(ret, "Error in allocating DMA buffers on capture plane", cleanup);

        /* Query the status of requested buffers
        ** to map the FDs to underlying Argus Buffers.
        ** Note: Application must call this for DMABUF.
        */
        for (uint32_t i = 0; i < ctx.capplane.num_buffers; ++i)
        {
            memset(&capplane_v4l2_buf, 0, sizeof (struct v4l2_buffer));
            memset(captureplanes, 0, sizeof (struct v4l2_plane));
            capplane_v4l2_buf.index = i;
            capplane_v4l2_buf.type = ctx.capplane.buf_type;
            capplane_v4l2_buf.memory = ctx.capplane.mem_type;
            capplane_v4l2_buf.m.planes = captureplanes;
            capplane_v4l2_buf.length = ctx.capplane.num_planes;
            capplane_v4l2_buf.m.planes[0].m.fd = ctx.dmabuffers_fd[i];

            ret = v4l2_ioctl(ctx.fd, VIDIOC_QUERYBUF, &capplane_v4l2_buf);
            CHECK_ERROR(ret, "Error in querying for "<< i <<
                "th buffer captureplane", cleanup);
        }
    }
    else
    {
        ret = req_buffers_on_capture_plane(&ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
            ctx.capplane.mem_type, 10);
        CHECK_ERROR(ret, "Error in requesting buffers on capture plane", cleanup);

        /* Query the status of requested buffers
        ** For each requested buffer, export buffer
        ** and map it for MMAP memory.
        */

        for (uint32_t i = 0; i < ctx.capplane.num_buffers; ++i)
        {
            memset(&capplane_v4l2_buf, 0, sizeof (struct v4l2_buffer));
            memset(captureplanes, 0, sizeof (struct v4l2_plane));
            capplane_v4l2_buf.index = i;
            capplane_v4l2_buf.type = ctx.capplane.buf_type;
            capplane_v4l2_buf.memory = ctx.capplane.mem_type;
            capplane_v4l2_buf.m.planes = captureplanes;
            capplane_v4l2_buf.length = ctx.capplane.num_planes;

            ret = v4l2_ioctl(ctx.fd, VIDIOC_QUERYBUF, &capplane_v4l2_buf);
            CHECK_ERROR(ret, "Error in querying for "<< i <<
                "th buffer captureplane", cleanup);

            for (uint32_t j = 0; j < capplane_v4l2_buf.length; ++j)
            {
                ctx.capplane.buffers[i]->planes[j].length =
                    capplane_v4l2_buf.m.planes[j].length;
                ctx.capplane.buffers[i]->planes[j].mem_offset =
                    capplane_v4l2_buf.m.planes[j].m.mem_offset;
            }

            memset(&capplane_expbuf, 0, sizeof (struct v4l2_exportbuffer));
            capplane_expbuf.type = ctx.capplane.buf_type;
            capplane_expbuf.index = i;

            for (uint32_t j = 0; j < ctx.capplane.num_planes; ++j)
            {
                capplane_expbuf.plane = j;
                ret = v4l2_ioctl(ctx.fd, VIDIOC_EXPBUF, &capplane_expbuf);
                CHECK_ERROR(ret, "Error in exporting "<< i <<
                    "th index buffer captureplane", cleanup);

                ctx.capplane.buffers[i]->planes[j].fd = capplane_expbuf.fd;
            }

            if (ctx.capplane.buffers[i]->map())
            {
                cerr << "Buffer mapping error on capture plane" << endl;
                ctx.in_error = 1;
                goto cleanup;
            }
        }
    }

    /* Set streaming on plane
    ** Start stream processing on capture
    ** plane by setting the streaming status ON.
    */

    ret = v4l2_ioctl (ctx.fd, VIDIOC_STREAMON, &ctx.capplane.buf_type);
    CHECK_ERROR(ret, "Error in setting streaming status ON capture plane", cleanup);

    ctx.capplane.streamon = 1;

    /* Enqueue all the empty buffers on capture plane. */
    if (ctx.capplane.mem_type != V4L2_MEMORY_DMABUF)
    {
        for (uint32_t i = 0; i < ctx.capplane.num_buffers; ++i)
        {
            struct v4l2_buffer queue_cap_v4l2_buf;
            struct v4l2_plane queue_cap_planes[MAX_PLANES];
            Buffer *buffer;

            memset(&queue_cap_v4l2_buf, 0, sizeof (struct v4l2_buffer));
            memset(queue_cap_planes, 0, MAX_PLANES * sizeof (struct v4l2_plane));

            buffer = ctx.capplane.buffers[i];
            queue_cap_v4l2_buf.index = i;
            queue_cap_v4l2_buf.m.planes = queue_cap_planes;

            ret = q_buffer(&ctx, queue_cap_v4l2_buf, buffer, ctx.capplane.buf_type,
                    ctx.capplane.mem_type, ctx.capplane.num_planes);
            CHECK_ERROR(ret, "Error while queueing buffer on capture plane", cleanup);
        }
    }
    else
    {
        for (uint32_t i = 0; i < ctx.capplane.num_buffers; ++i)
        {
            struct v4l2_buffer queue_cap_v4l2_buf;
            struct v4l2_plane queue_cap_planes[MAX_PLANES];

            memset(&queue_cap_v4l2_buf, 0, sizeof (struct v4l2_buffer));
            memset(queue_cap_planes, 0, MAX_PLANES * sizeof (struct v4l2_plane));

            queue_cap_v4l2_buf.index = i;
            queue_cap_v4l2_buf.m.planes = queue_cap_planes;
            queue_cap_v4l2_buf.m.planes[0].m.fd = ctx.dmabuffers_fd[i];

            ret = q_buffer(&ctx, queue_cap_v4l2_buf, NULL, ctx.capplane.buf_type,
                    ctx.capplane.mem_type, ctx.capplane.num_planes);
            CHECK_ERROR(ret, "Error while queueing buffer on capture plane", cleanup);
        }
    }

    /* Create DQ Capture loop thread
    ** and set the callback function to dq_thread.
    */
    pthread_mutex_lock(&ctx.queue_lock);
    ctx.dqthread_running = true;
    pthread_create(&ctx.cam_dq_thread, NULL, dq_thread, &ctx);
    pthread_mutex_unlock(&ctx.queue_lock);

    /* For blocking mode, wait till all the buffers
    ** are successfully  DQed from the capture plane.
    */
    wait_for_dqthread(ctx, -1);

    /* Cleanup and exit. */

cleanup:
    if (ctx.fd != -1)
    {

        /* Stream off */
        ret = v4l2_ioctl(ctx.fd, VIDIOC_STREAMOFF, &ctx.capplane.buf_type);
        ctx.capplane.streamon = 0;

        if (ctx.capplane.mem_type != V4L2_MEMORY_DMABUF)
        {
            /* Unmap MMAPed buffers. */
            for (uint32_t i = 0; i < ctx.capplane.num_buffers; ++i)
            {
                ctx.capplane.buffers[i]->unmap();
            }
            /* Request 0 buffers on capture plane. */
            ret = req_buffers_on_capture_plane(&ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
                    ctx.capplane.mem_type, 0);
        }
        else
        {
            /* Request 0 buffers on capture plane. */
            ret = allocate_dmabuffers(&ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
                    ctx.capplane.mem_type, 0);
        }

        /* Close the opened V4L2 device. */

        ret = v4l2_close(ctx.fd);
        if (ret)
        {
            cerr << "Unable to close the device" << endl;
            ctx.in_error = 1;
        }

    }

    if (ctx.output_file)
    {
        ctx.output_file->close();
        delete ctx.output_file;
    }
    if (ctx.output_file_path)
    {
        free(ctx.output_file_path);
    }

    if (!ctx.disable_rendering && ctx.display.renderer)
    {
        delete ctx.display.renderer;
        ctx.display.renderer = NULL;
    }

    if (ctx.in_error)
        return -1;

    return 0;
}


int main (int argc, char const *argv[])
{
    context_t ctx;
    int ret = 0;
    int iterator_num = 0;
    do
    {
        if (iterator_num)
            cout << "Iteration " << iterator_num << endl;

        ret = capture_proc (ctx, argc, argv);
        iterator_num++;
    } while (ctx.stress_test != iterator_num && ret == 0 && !quit_capture);

    if (ret)
        cerr << "Camera is in error" << endl;
    else
        cout << "Camera Run Successful" << endl;

    return ret;
}
