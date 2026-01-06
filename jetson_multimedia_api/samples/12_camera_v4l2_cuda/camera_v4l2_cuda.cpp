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

#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <poll.h>

#include "NvEglRenderer.h"
#include "NvUtils.h"
#include "NvCudaProc.h"
#include "nvbuf_utils.h"

#include "camera_v4l2_cuda.h"

#define MJPEG_EOS_SEARCH_SIZE 4096

static bool quit = false;

using namespace std;

static void
print_usage(void)
{
    printf("\n\tUsage: camera_v4l2_cuda [OPTIONS]\n\n"
           "\tExample: \n"
           "\t./camera_v4l2_cuda -d /dev/video0 -s 640x480 -f YUYV -n 30 -c\n\n"
           "\tSupported options:\n"
           "\t-d\t\tSet V4l2 video device node\n"
           "\t-s\t\tSet output resolution of video device\n"
           "\t-f\t\tSet output pixel format of video device (supports only YUYV/YVYU/UYVY/VYUY/GREY/MJPEG)\n"
           "\t-r\t\tSet renderer frame rate (30 fps by default)\n"
           "\t-n\t\tSave the n-th frame before VIC processing\n"
           "\t-c\t\tEnable CUDA aglorithm (draw a black box in the upper left corner)\n"
           "\t-v\t\tEnable verbose message\n"
           "\t-h\t\tPrint this usage\n\n"
           "\tNOTE: It runs infinitely until you terminate it with <ctrl+c>\n");
}

static bool
parse_cmdline(context_t * ctx, int argc, char **argv)
{
    int c;

    if (argc < 2)
    {
        print_usage();
        exit(EXIT_SUCCESS);
    }

    while ((c = getopt(argc, argv, "d:s:f:r:n:cvh")) != -1)
    {
        switch (c)
        {
            case 'd':
                ctx->cam_devname = optarg;
                break;
            case 's':
                if (sscanf(optarg, "%dx%d",
                            &ctx->cam_w, &ctx->cam_h) != 2)
                {
                    print_usage();
                    return false;
                }
                break;
            case 'f':
                if (strcmp(optarg, "YUYV") == 0)
                    ctx->cam_pixfmt = V4L2_PIX_FMT_YUYV;
                else if (strcmp(optarg, "YVYU") == 0)
                    ctx->cam_pixfmt = V4L2_PIX_FMT_YVYU;
                else if (strcmp(optarg, "VYUY") == 0)
                    ctx->cam_pixfmt = V4L2_PIX_FMT_VYUY;
                else if (strcmp(optarg, "UYVY") == 0)
                    ctx->cam_pixfmt = V4L2_PIX_FMT_UYVY;
                else if (strcmp(optarg, "GREY") == 0)
                    ctx->cam_pixfmt = V4L2_PIX_FMT_GREY;
                else if (strcmp(optarg, "MJPEG") == 0)
                    ctx->cam_pixfmt = V4L2_PIX_FMT_MJPEG;
                else
                {
                    print_usage();
                    return false;
                }
                sprintf(ctx->cam_file, "camera.%s", optarg);
                break;
            case 'r':
                ctx->fps = strtol(optarg, NULL, 10);
                break;
            case 'n':
                ctx->save_n_frame = strtol(optarg, NULL, 10);
                break;
            case 'c':
                ctx->enable_cuda = true;
                break;
            case 'v':
                ctx->enable_verbose = true;
                break;
            case 'h':
                print_usage();
                exit(EXIT_SUCCESS);
                break;
            default:
                print_usage();
                return false;
        }
    }

    return true;
}

static void
set_defaults(context_t * ctx)
{
    memset(ctx, 0, sizeof(context_t));

    ctx->cam_devname = "/dev/video0";
    ctx->cam_fd = -1;
    ctx->cam_pixfmt = V4L2_PIX_FMT_YUYV;
    ctx->cam_w = 640;
    ctx->cam_h = 480;
    ctx->frame = 0;
    ctx->save_n_frame = 0;

    ctx->g_buff = NULL;
    ctx->capture_dmabuf = true;
    ctx->renderer = NULL;
    ctx->fps = 30;

    ctx->enable_cuda = false;
    ctx->egl_image = NULL;
    ctx->egl_display = EGL_NO_DISPLAY;

    ctx->enable_verbose = false;
}

static nv_color_fmt nvcolor_fmt[] =
{
    /* TODO: add more pixel format mapping */
    {V4L2_PIX_FMT_UYVY, NvBufferColorFormat_UYVY},
    {V4L2_PIX_FMT_VYUY, NvBufferColorFormat_VYUY},
    {V4L2_PIX_FMT_YUYV, NvBufferColorFormat_YUYV},
    {V4L2_PIX_FMT_YVYU, NvBufferColorFormat_YVYU},
    {V4L2_PIX_FMT_GREY, NvBufferColorFormat_GRAY8},
    {V4L2_PIX_FMT_YUV420M, NvBufferColorFormat_YUV420},
};

static NvBufferColorFormat
get_nvbuff_color_fmt(unsigned int v4l2_pixfmt)
{
    unsigned i;

    for (i = 0; i < sizeof(nvcolor_fmt) / sizeof(nvcolor_fmt[0]); i++)
    {
        if (v4l2_pixfmt == nvcolor_fmt[i].v4l2_pixfmt)
            return nvcolor_fmt[i].nvbuff_color;
    }

    return NvBufferColorFormat_Invalid;
}

static bool
save_frame_to_file(context_t * ctx, struct v4l2_buffer * buf)
{
    int file;

    file = open(ctx->cam_file, O_CREAT | O_WRONLY | O_APPEND | O_TRUNC,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    if (-1 == file)
        ERROR_RETURN("Failed to open file for frame saving");

    if (-1 == write(file, ctx->g_buff[buf->index].start,
                ctx->g_buff[buf->index].size))
    {
        close(file);
        ERROR_RETURN("Failed to write frame into file");
    }

    close(file);

    return true;
}

static bool
nvbuff_do_clearchroma (int dmabuf_fd)
{
  NvBufferParams params = {0};
  void *sBaseAddr[3] = {NULL};
  int ret = 0;
  int size;
  unsigned i;

  ret = NvBufferGetParams (dmabuf_fd, &params);
  if (ret != 0)
    ERROR_RETURN("%s: NvBufferGetParams Failed \n", __func__);

  for (i = 1; i < params.num_planes; i++) {
    ret = NvBufferMemMap (dmabuf_fd, i, NvBufferMem_Read_Write, &sBaseAddr[i]);
    if (ret != 0)
      ERROR_RETURN("%s: NvBufferMemMap Failed \n", __func__);

    /* Sync device cache for CPU access since data is from VIC */
    ret = NvBufferMemSyncForCpu (dmabuf_fd, i, &sBaseAddr[i]);
    if (ret != 0)
      ERROR_RETURN("%s: NvBufferMemSyncForCpu Failed \n", __func__);

    size = params.height[i] * params.pitch[i];
    memset (sBaseAddr[i], 0x80, size);

    /* Sync CPU cache for VIC access since data is from CPU */
    ret = NvBufferMemSyncForDevice (dmabuf_fd, i, &sBaseAddr[i]);
    if (ret != 0)
      ERROR_RETURN("%s: NvBufferMemSyncForDevice Failed \n", __func__);

    ret = NvBufferMemUnMap (dmabuf_fd, i, &sBaseAddr[i]);
    if (ret != 0)
      ERROR_RETURN("%s: NvBufferMemUnMap Failed \n", __func__);
  }

  return true;
}

static bool
camera_initialize(context_t * ctx)
{
    struct v4l2_format fmt;

    /* Open camera device */
    ctx->cam_fd = open(ctx->cam_devname, O_RDWR);
    if (ctx->cam_fd == -1)
        ERROR_RETURN("Failed to open camera device %s: %s (%d)",
                ctx->cam_devname, strerror(errno), errno);

    /* Set camera output format */
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = ctx->cam_w;
    fmt.fmt.pix.height = ctx->cam_h;
    fmt.fmt.pix.pixelformat = ctx->cam_pixfmt;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    if (ioctl(ctx->cam_fd, VIDIOC_S_FMT, &fmt) < 0)
        ERROR_RETURN("Failed to set camera output format: %s (%d)",
                strerror(errno), errno);

    /* Get the real format in case the desired is not supported */
    memset(&fmt, 0, sizeof fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(ctx->cam_fd, VIDIOC_G_FMT, &fmt) < 0)
        ERROR_RETURN("Failed to get camera output format: %s (%d)",
                strerror(errno), errno);
    if (fmt.fmt.pix.width != ctx->cam_w ||
            fmt.fmt.pix.height != ctx->cam_h ||
            fmt.fmt.pix.pixelformat != ctx->cam_pixfmt)
    {
        WARN("The desired format is not supported");
        ctx->cam_w = fmt.fmt.pix.width;
        ctx->cam_h = fmt.fmt.pix.height;
        ctx->cam_pixfmt =fmt.fmt.pix.pixelformat;
    }

    struct v4l2_streamparm streamparm;
    memset (&streamparm, 0x00, sizeof (struct v4l2_streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl (ctx->cam_fd, VIDIOC_G_PARM, &streamparm);

    INFO("Camera ouput format: (%d x %d)  stride: %d, imagesize: %d, frate: %u / %u",
            fmt.fmt.pix.width,
            fmt.fmt.pix.height,
            fmt.fmt.pix.bytesperline,
            fmt.fmt.pix.sizeimage,
            streamparm.parm.capture.timeperframe.denominator,
            streamparm.parm.capture.timeperframe.numerator);

    return true;
}

static bool
display_initialize(context_t * ctx)
{
    /* Create EGL renderer */
    ctx->renderer = NvEglRenderer::createEglRenderer("renderer0",
            ctx->cam_w, ctx->cam_h, 0, 0);
    if (!ctx->renderer)
        ERROR_RETURN("Failed to create EGL renderer");
    ctx->renderer->setFPS(ctx->fps);

    if (ctx->enable_cuda)
    {
        /* Get defalut EGL display */
        ctx->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (ctx->egl_display == EGL_NO_DISPLAY)
            ERROR_RETURN("Failed to get EGL display connection");

        /* Init EGL display connection */
        if (!eglInitialize(ctx->egl_display, NULL, NULL))
            ERROR_RETURN("Failed to initialize EGL display connection");
    }

    return true;
}

static bool
init_components(context_t * ctx)
{
    if (!camera_initialize(ctx))
        ERROR_RETURN("Failed to initialize camera device");

    if (!display_initialize(ctx))
        ERROR_RETURN("Failed to initialize display");

    INFO("Initialize v4l2 components successfully");
    return true;
}

static bool
request_camera_buff(context_t *ctx)
{
    /* Request camera v4l2 buffer */
    struct v4l2_requestbuffers rb;
    memset(&rb, 0, sizeof(rb));
    rb.count = V4L2_BUFFERS_NUM;
    rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rb.memory = V4L2_MEMORY_DMABUF;
    if (ioctl(ctx->cam_fd, VIDIOC_REQBUFS, &rb) < 0)
        ERROR_RETURN("Failed to request v4l2 buffers: %s (%d)",
                strerror(errno), errno);
    if (rb.count != V4L2_BUFFERS_NUM)
        ERROR_RETURN("V4l2 buffer number is not as desired");

    for (unsigned int index = 0; index < V4L2_BUFFERS_NUM; index++)
    {
        struct v4l2_buffer buf;

        /* Query camera v4l2 buf length */
        memset(&buf, 0, sizeof buf);
        buf.index = index;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_DMABUF;

        if (ioctl(ctx->cam_fd, VIDIOC_QUERYBUF, &buf) < 0)
            ERROR_RETURN("Failed to query buff: %s (%d)",
                    strerror(errno), errno);

        /* TODO: add support for multi-planer
           Enqueue empty v4l2 buff into camera capture plane */
        buf.m.fd = (unsigned long)ctx->g_buff[index].dmabuff_fd;
        if (buf.length != ctx->g_buff[index].size)
        {
            WARN("Camera v4l2 buf length is not expected");
            ctx->g_buff[index].size = buf.length;
        }

        if (ioctl(ctx->cam_fd, VIDIOC_QBUF, &buf) < 0)
            ERROR_RETURN("Failed to enqueue buffers: %s (%d)",
                    strerror(errno), errno);
    }

    return true;
}

static bool
request_camera_buff_mmap(context_t *ctx)
{
    /* Request camera v4l2 buffer */
    struct v4l2_requestbuffers rb;
    memset(&rb, 0, sizeof(rb));
    rb.count = V4L2_BUFFERS_NUM;
    rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rb.memory = V4L2_MEMORY_MMAP;
    if (ioctl(ctx->cam_fd, VIDIOC_REQBUFS, &rb) < 0)
        ERROR_RETURN("Failed to request v4l2 buffers: %s (%d)",
                strerror(errno), errno);
    if (rb.count != V4L2_BUFFERS_NUM)
        ERROR_RETURN("V4l2 buffer number is not as desired");

    for (unsigned int index = 0; index < V4L2_BUFFERS_NUM; index++)
    {
        struct v4l2_buffer buf;

        /* Query camera v4l2 buf length */
        memset(&buf, 0, sizeof buf);
        buf.index = index;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(ctx->cam_fd, VIDIOC_QUERYBUF, &buf) < 0)
            ERROR_RETURN("Failed to query buff: %s (%d)",
                    strerror(errno), errno);

        ctx->g_buff[index].size = buf.length;
        ctx->g_buff[index].start = (unsigned char *)
            mmap (NULL /* start anywhere */,
                    buf.length,
                    PROT_READ | PROT_WRITE /* required */,
                    MAP_SHARED /* recommended */,
                    ctx->cam_fd, buf.m.offset);
        if (MAP_FAILED == ctx->g_buff[index].start)
            ERROR_RETURN("Failed to map buffers");

        if (ioctl(ctx->cam_fd, VIDIOC_QBUF, &buf) < 0)
            ERROR_RETURN("Failed to enqueue buffers: %s (%d)",
                    strerror(errno), errno);
    }

    return true;
}

static bool
prepare_buffers_mjpeg(context_t * ctx)
{
    NvBufferCreateParams input_params = {0};

    /* Allocate global buffer context */
    ctx->g_buff = (nv_buffer *)malloc(V4L2_BUFFERS_NUM * sizeof(nv_buffer));
    if (ctx->g_buff == NULL)
        ERROR_RETURN("Failed to allocate global buffer context");
    memset(ctx->g_buff, 0, V4L2_BUFFERS_NUM * sizeof(nv_buffer));

    input_params.payloadType = NvBufferPayload_SurfArray;
    input_params.width = ctx->cam_w;
    input_params.height = ctx->cam_h;
    input_params.layout = NvBufferLayout_Pitch;
    input_params.colorFormat = get_nvbuff_color_fmt(V4L2_PIX_FMT_YUV420M);
    input_params.nvbuf_tag = NvBufferTag_NONE;

    /* Create Render buffer */
    if (-1 == NvBufferCreateEx(&ctx->render_dmabuf_fd, &input_params))
        ERROR_RETURN("Failed to create NvBuffer");

    ctx->capture_dmabuf = false;
    if (!request_camera_buff_mmap(ctx))
        ERROR_RETURN("Failed to set up camera buff");

    INFO("Succeed in preparing mjpeg buffers");
    return true;
}

static bool
prepare_buffers(context_t * ctx)
{
    NvBufferCreateParams input_params = {0};

    /* Allocate global buffer context */
    ctx->g_buff = (nv_buffer *)malloc(V4L2_BUFFERS_NUM * sizeof(nv_buffer));
    if (ctx->g_buff == NULL)
        ERROR_RETURN("Failed to allocate global buffer context");

    input_params.payloadType = NvBufferPayload_SurfArray;
    input_params.width = ctx->cam_w;
    input_params.height = ctx->cam_h;
    input_params.layout = NvBufferLayout_Pitch;

    /* Create buffer and provide it with camera */
    for (unsigned int index = 0; index < V4L2_BUFFERS_NUM; index++)
    {
        int fd;
        NvBufferParams params = {0};

        input_params.colorFormat = get_nvbuff_color_fmt(ctx->cam_pixfmt);
        input_params.nvbuf_tag = NvBufferTag_CAMERA;
        if (-1 == NvBufferCreateEx(&fd, &input_params))
            ERROR_RETURN("Failed to create NvBuffer");

        ctx->g_buff[index].dmabuff_fd = fd;

        if (-1 == NvBufferGetParams(fd, &params))
            ERROR_RETURN("Failed to get NvBuffer parameters");

        if (ctx->cam_pixfmt == V4L2_PIX_FMT_GREY &&
            params.pitch[0] != params.width[0])
                ctx->capture_dmabuf = false;

        /* TODO: add multi-planar support
           Currently only supports YUV422 interlaced single-planar */
        if (ctx->capture_dmabuf) {
            if (-1 == NvBufferMemMap(ctx->g_buff[index].dmabuff_fd, 0, NvBufferMem_Read_Write,
                        (void**)&ctx->g_buff[index].start))
                ERROR_RETURN("Failed to map buffer");
        }
    }

    input_params.colorFormat = get_nvbuff_color_fmt(V4L2_PIX_FMT_YUV420M);
    input_params.nvbuf_tag = NvBufferTag_NONE;
    /* Create Render buffer */
    if (-1 == NvBufferCreateEx(&ctx->render_dmabuf_fd, &input_params))
        ERROR_RETURN("Failed to create NvBuffer");

    if (ctx->capture_dmabuf) {
        if (!request_camera_buff(ctx))
            ERROR_RETURN("Failed to set up camera buff");
    } else {
        if (!request_camera_buff_mmap(ctx))
            ERROR_RETURN("Failed to set up camera buff");
    }

    INFO("Succeed in preparing stream buffers");
    return true;
}

static bool
start_stream(context_t * ctx)
{
    enum v4l2_buf_type type;

    /* Start v4l2 streaming */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(ctx->cam_fd, VIDIOC_STREAMON, &type) < 0)
        ERROR_RETURN("Failed to start streaming: %s (%d)",
                strerror(errno), errno);

    usleep(200);

    INFO("Camera video streaming on ...");
    return true;
}

static void
signal_handle(int signum)
{
    printf("Quit due to exit command from user!\n");
    quit = true;
}

static bool
cuda_postprocess(context_t *ctx, int fd)
{
    if (ctx->enable_cuda)
    {
        /* Create EGLImage from dmabuf fd */
        ctx->egl_image = NvEGLImageFromFd(ctx->egl_display, fd);
        if (ctx->egl_image == NULL)
            ERROR_RETURN("Failed to map dmabuf fd (0x%X) to EGLImage",
                    ctx->render_dmabuf_fd);

        /* Pass this buffer hooked on this egl_image to CUDA for
           CUDA processing - draw a rectangle on the frame */
        HandleEGLImage(&ctx->egl_image);

        /* Destroy EGLImage */
        NvDestroyEGLImage(ctx->egl_display, ctx->egl_image);
        ctx->egl_image = NULL;
    }

    return true;
}

static bool
start_capture(context_t * ctx)
{
    struct sigaction sig_action;
    struct pollfd fds[1];
    NvBufferTransformParams transParams;

    /* Register a shuwdown handler to ensure
       a clean shutdown if user types <ctrl+c> */
    sig_action.sa_handler = signal_handle;
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags = 0;
    sigaction(SIGINT, &sig_action, NULL);

    if (ctx->cam_pixfmt == V4L2_PIX_FMT_MJPEG)
        ctx->jpegdec = NvJPEGDecoder::createJPEGDecoder("jpegdec");

    /* Init the NvBufferTransformParams */
    memset(&transParams, 0, sizeof(transParams));
    transParams.transform_flag = NVBUFFER_TRANSFORM_FILTER;
    transParams.transform_filter = NvBufferTransform_Filter_Smart;

    /* Enable render profiling information */
    ctx->renderer->enableProfiling();

    fds[0].fd = ctx->cam_fd;
    fds[0].events = POLLIN;
    /* Wait for camera event with timeout = 5000 ms */
    while (poll(fds, 1, 5000) > 0 && !quit)
    {
        if (fds[0].revents & POLLIN) {
            struct v4l2_buffer v4l2_buf;

            /* Dequeue a camera buff */
            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (ctx->capture_dmabuf)
                v4l2_buf.memory = V4L2_MEMORY_DMABUF;
            else
                v4l2_buf.memory = V4L2_MEMORY_MMAP;
            if (ioctl(ctx->cam_fd, VIDIOC_DQBUF, &v4l2_buf) < 0)
                ERROR_RETURN("Failed to dequeue camera buff: %s (%d)",
                        strerror(errno), errno);

            ctx->frame++;

            /* Save the n-th frame to file */
            if (ctx->frame == ctx->save_n_frame)
                save_frame_to_file(ctx, &v4l2_buf);

            if (ctx->cam_pixfmt == V4L2_PIX_FMT_MJPEG) {
                int fd = 0;
                uint32_t width, height, pixfmt;
                unsigned int i = 0;
                unsigned int eos_search_size = MJPEG_EOS_SEARCH_SIZE;
                unsigned int bytesused = v4l2_buf.bytesused;
                uint8_t *p;

                /* v4l2_buf.bytesused may have padding bytes for alignment
                   Search for EOF to get exact size */
                if (eos_search_size > bytesused)
                    eos_search_size = bytesused;
                for (i = 0; i < eos_search_size; i++) {
                    p =(uint8_t *)(ctx->g_buff[v4l2_buf.index].start + bytesused);
                    if ((*(p-2) == 0xff) && (*(p-1) == 0xd9)) {
                        break;
                    }
                    bytesused--;
                }

                /* Decoding MJPEG frame */
                if (ctx->jpegdec->decodeToFd(fd, ctx->g_buff[v4l2_buf.index].start,
                    bytesused, pixfmt, width, height) < 0)
                    ERROR_RETURN("Cannot decode MJPEG");

                /* Convert the decoded buffer to YUV420P */
                if (-1 == NvBufferTransform(fd, ctx->render_dmabuf_fd,
                        &transParams))
                    ERROR_RETURN("Failed to convert the buffer");
            } else {
                if (ctx->capture_dmabuf) {
                    /* Cache sync for VIC operation since the data is from CPU */
                    NvBufferMemSyncForDevice(ctx->g_buff[v4l2_buf.index].dmabuff_fd, 0,
                            (void**)&ctx->g_buff[v4l2_buf.index].start);
                } else {
                    /* Copies raw buffer plane contents to an NvBuffer plane */
                    Raw2NvBuffer(ctx->g_buff[v4l2_buf.index].start, 0,
                             ctx->cam_w, ctx->cam_h, ctx->g_buff[v4l2_buf.index].dmabuff_fd);
                }

                /*  Convert the camera buffer from YUV422 to YUV420P */
                if (-1 == NvBufferTransform(ctx->g_buff[v4l2_buf.index].dmabuff_fd, ctx->render_dmabuf_fd,
                            &transParams))
                    ERROR_RETURN("Failed to convert the buffer");

                if (ctx->cam_pixfmt == V4L2_PIX_FMT_GREY) {
                    if(!nvbuff_do_clearchroma(ctx->render_dmabuf_fd))
                        ERROR_RETURN("Failed to clear chroma");
                }
            }
            cuda_postprocess(ctx, ctx->render_dmabuf_fd);

            /* Preview */
            ctx->renderer->render(ctx->render_dmabuf_fd);

            /* Enqueue camera buffer back to driver */
            if (ioctl(ctx->cam_fd, VIDIOC_QBUF, &v4l2_buf))
                ERROR_RETURN("Failed to queue camera buffers: %s (%d)",
                        strerror(errno), errno);
        }
    }

    /* Print profiling information when streaming stops */
    ctx->renderer->printProfilingStats();

    if (ctx->cam_pixfmt == V4L2_PIX_FMT_MJPEG)
        delete ctx->jpegdec;

    return true;
}

static bool
stop_stream(context_t * ctx)
{
    enum v4l2_buf_type type;

    /* Stop v4l2 streaming */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(ctx->cam_fd, VIDIOC_STREAMOFF, &type))
        ERROR_RETURN("Failed to stop streaming: %s (%d)",
                strerror(errno), errno);

    INFO("Camera video streaming off ...");
    return true;
}

int
main(int argc, char *argv[])
{
    context_t ctx;
    int error = 0;

    set_defaults(&ctx);

    CHECK_ERROR(parse_cmdline(&ctx, argc, argv), cleanup,
            "Invalid options specified");

    /* Initialize camera and EGL display, EGL Display will be used to map
       the buffer to CUDA buffer for CUDA processing */
    CHECK_ERROR(init_components(&ctx), cleanup,
            "Failed to initialize v4l2 components");

    if (ctx.cam_pixfmt == V4L2_PIX_FMT_MJPEG) {
        CHECK_ERROR(prepare_buffers_mjpeg(&ctx), cleanup,
                "Failed to prepare v4l2 buffs");
    } else {
        CHECK_ERROR(prepare_buffers(&ctx), cleanup,
                "Failed to prepare v4l2 buffs");
    }

    CHECK_ERROR(start_stream(&ctx), cleanup,
            "Failed to start streaming");

    CHECK_ERROR(start_capture(&ctx), cleanup,
            "Failed to start capturing")

    CHECK_ERROR(stop_stream(&ctx), cleanup,
            "Failed to stop streaming");

cleanup:
    if (ctx.cam_fd > 0)
        close(ctx.cam_fd);

    if (ctx.renderer != NULL)
        delete ctx.renderer;

    if (ctx.egl_display && !eglTerminate(ctx.egl_display))
        printf("Failed to terminate EGL display connection\n");

    if (ctx.g_buff != NULL)
    {
        for (unsigned i = 0; i < V4L2_BUFFERS_NUM; i++) {
            if (ctx.g_buff[i].dmabuff_fd)
                NvBufferDestroy(ctx.g_buff[i].dmabuff_fd);
            if (ctx.cam_pixfmt == V4L2_PIX_FMT_MJPEG)
                munmap(ctx.g_buff[i].start, ctx.g_buff[i].size);
        }
        free(ctx.g_buff);
    }

    NvBufferDestroy(ctx.render_dmabuf_fd);

    if (error)
        printf("App run failed\n");
    else
        printf("App run was successful\n");

    return -error;
}
