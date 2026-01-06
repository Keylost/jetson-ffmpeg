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

#include <errno.h>
#include <fstream>
#include <iostream>
#include <linux/videodev2.h>
#include <malloc.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <drm_fourcc.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "NvUtils.h"
#include "video_dec_drm.h"
#include "tegra_drm.h"
#ifndef DOWNSTREAM_TEGRA_DRM
#include "tegra_drm_nvdc.h"
#endif
#include "NvApplicationProfiler.h"
#include "nvbuf_utils.h"

#define TEST_ERROR(cond, str, label) \
    if(cond) \
    { \
        cerr << str << endl; \
        error = 1; \
        goto label; \
    }

#define CHUNK_SIZE 4000000
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#define INVALID_PLANE 0xFFFF
#define ZERO_FD 0x0
#define UI_FROMAT_ARGB2101010 0

using namespace std;

/* defined in generated image_rgba.cpp */
extern unsigned int image_w;
extern unsigned int image_h;
extern char image_pixels_array[];

unordered_map <int, int> fd_map;

static void leave_vt(context_t * ctx)
{
    int ret;

    ret = ioctl(ctx->console_fd, KDSETMODE, KD_TEXT);
    if (ret < 0) {
        printf("KDSETMODE failed, err=%s\n", strerror(errno));
    }

    if (ctx->active_vt >= 0) {
        ret = ioctl(ctx->console_fd, VT_ACTIVATE, ctx->active_vt);
        if (ret < 0) {
            printf("VT_ACTIVATE failed, err=%s\n", strerror(errno));
        }

        ret = ioctl(ctx->console_fd, VT_WAITACTIVE, ctx->active_vt);
        if (ret < 0) {
            printf("VT_WAITACTIVE failed, err= %s\n", strerror(errno));
        }
    }

    close(ctx->console_fd);
    ctx->console_fd = -1;
    ctx->active_vt = -1;
}

static void enter_vt(context_t * ctx)
{
    int i, ret, fd, vtno;
    struct vt_stat vts;
    const char *vcs[] = { "/dev/vc/%d", "/dev/tty%d", NULL };
    static char vtname[11];

    fd = open("/dev/tty0", O_WRONLY, 0);
    if (fd < 0) {
        printf("can't open /dev/tty0 err=%s\n", strerror(errno));
        return;
    }

    ret = ioctl(fd, VT_OPENQRY, &vtno);
    if (ret < 0) {
        printf("VT_OPENQRY failed, err=%s\n", strerror(errno));
        close(fd);
        return;
    }

    if (vtno == -1) {
        printf("can't find free VT\n");
        close(fd);
        return;
    }

    printf("Using VT number %d\n", vtno);
    close(fd);

    i = 0;
    while (vcs[i] != NULL) {
        snprintf(vtname, sizeof(vtname), vcs[i], vtno);
        ctx->console_fd = open(vtname, O_RDWR | O_NDELAY, 0);
        if (ctx->console_fd >= 0) {
            break;
        }
        i++;
    }

    if (ctx->console_fd < 0) {
        printf("can't open virtual console %d\n", vtno);
    }

    ret = ioctl(ctx->console_fd, VT_GETSTATE, &vts);
    if (ret < 0) {
        printf("VT_GETSTATE failed, err=%s\n", strerror(errno));
    } else {
        ctx->active_vt = vts.v_active;
    }

    ret = ioctl(ctx->console_fd, VT_ACTIVATE, vtno);
    if (ret < 0) {
        printf("VT_ACTIVATE failed, err=%s\n", strerror(errno));
        return;
    }

    ret = ioctl(ctx->console_fd, VT_WAITACTIVE, vtno);
    if (ret < 0) {
        printf("VT_WAITACTIVE failed, err=%s\n", strerror(errno));
        return;
    }

    ret = ioctl(ctx->console_fd, KDSETMODE, KD_GRAPHICS);
    if (ret < 0) {
        printf("KDSETMODE KD_GRAPHICS failed, err=%s\n", strerror(errno));
    }

    return;
}

static int
read_decoder_input_chunk(ifstream * stream, NvBuffer * buffer)
{
    /* Length is the size of the buffer in bytes */
    streamsize bytes_to_read = MIN(CHUNK_SIZE, buffer->planes[0].length);

    stream->read((char *) buffer->planes[0].data, bytes_to_read);
    /**
     * It is necessary to set bytesused properly, so that decoder knows how
     * many bytes in the buffer are valid
     */
    buffer->planes[0].bytesused = stream->gcount();
    return 0;
}

void
abort(context_t *ctx)
{
    ctx->got_error = true;
    ctx->dec->abort();
    leave_vt(ctx);
}

static void *
ui_render_loop_fcn(void *arg)
{
    context_t *ctx = (context_t *) arg;
    NvDrmFB ui_fb[3];
    uint32_t plane_count = ctx->drm_renderer->getPlaneCount();
    int32_t *plane_index = NULL;
    int32_t primary_plane_index = -1;
    int32_t overlay_plane_index = -1;
    /**
     * The variables 'image_pixels_array', 'image_w' and 'image_h'
     * are defined in the following auto-generated header file,
     * 'image_rgba.h'
     */
    const char *p = image_pixels_array;
    uint32_t ui_width = 200;
    uint32_t ui_height = 200;
    uint32_t frame = 0;
    long elapsed_us = 0;
#if UI_FROMAT_ARGB2101010
    uint32_t ui_format = DRM_FORMAT_ARGB2101010;
#else
    uint32_t ui_format = DRM_FORMAT_ARGB8888;
#endif

    /* Render a static JPEG image on the first plane */
    ctx->drm_renderer->createDumbFB(image_w, image_h,
            ui_format,
            &ui_fb[0]);
    for (uint32_t y = 0; y < image_h; ++y)
    {
        for (uint32_t x = 0; x < image_w; ++x)
        {
            uint32_t off = ui_fb[0].bo[0].pitch * y + x * 4;
            ui_fb[0].bo[0].data[off] = *p++;
            ui_fb[0].bo[0].data[off + 1] = *p++;
            ui_fb[0].bo[0].data[off + 2] = *p++;
            ui_fb[0].bo[0].data[off + 3] = *p++;
        }
    }

    plane_index = (int32_t*)malloc(plane_count * sizeof(int32_t));
    if (plane_index && ctx->drm_renderer->getPlaneIndex(ctx->crtc, plane_index)) {
        primary_plane_index = plane_index[0];
        overlay_plane_index = plane_index[1];
    }

    if (primary_plane_index == -1) {
        /**
         * It's kind of trick to distinguish the platforms with plane_count.
         * We'd better decide target window with the real hardware configuration.
         * By default,
         * TX1:
         *    CRTC 0: primary(win_A), overlay planes(win_B & win_C & win_D)
         *    CRTC 1: primary(win_A), overlay planes(win_B & win_C)
         * TX2:
         *    CRTC 0: primary(win_A), overlay planes(win_B & win_C)
         *    CRTC 1: primary(win_A), overlay planes(win_B)
         *    CRTC 2: primary(win_A)
         * NOTE: The plane_count implies the overlay windows
         */

        if (plane_count == 3)
            primary_plane_index = (ctx->crtc == 0) ? 0 : 2;
        else
            primary_plane_index = (ctx->crtc == 0) ? 0 : 3;
    }

    ctx->drm_renderer->setPlane(primary_plane_index, ui_fb[0].fb_id,
            0, 0, image_w, image_h,
            0, 0, image_w << 16, image_h << 16);

    /**
     * Moving color block on the second plane
     * The ui_fb[1] and ui_fb[2] are playing the roles of
     * double buffering to get rid of tearing issue.
     */
    for (uint32_t i = 1; i < 3; i++)
        ctx->drm_renderer->createDumbFB(ui_height, ui_width,
                ui_format,
                &ui_fb[i]);
    do {
        struct timeval begin, end;

        gettimeofday(&begin, NULL);

        for (uint32_t y = 0; y < ui_height; ++y)
        {
            for (uint32_t x = 0; x < ui_width; ++x)
            {
                uint32_t off = ui_fb[frame % 2 + 1].bo[0].pitch * y + x * 4;
                ui_fb[frame % 2 + 1].bo[0].data[off] = frame % 255;
                ui_fb[frame % 2 + 1].bo[0].data[off + 1] = (frame + 255 / 3) % 255;
                ui_fb[frame % 2 + 1].bo[0].data[off + 2] = (frame + 255 / 2)% 255;
                ui_fb[frame % 2 + 1].bo[0].data[off + 3] = x % 255;
            }
        }

        if (overlay_plane_index == -1) {
            /**
             * If plane_count is equal to 3, we don't have enough overlay to render
             * moving color block on the second crtc.
             */
            if (plane_count == 3)
                overlay_plane_index = (ctx->crtc == 0) ? 1 : INVALID_PLANE;
            else
                overlay_plane_index = (ctx->crtc == 0) ? 1 : 4;
        }

        /* The flip will be happening after vblank for the completed buffer */
        ctx->drm_renderer->setPlane(overlay_plane_index, ui_fb[frame % 2 + 1].fb_id,
                frame % image_w, frame % image_h, ui_width, ui_height,
                0, 0, ui_width << 16, ui_height << 16);

        frame++;

        /**
         * Get EOS signal from video capturing thread,
         * so setPlane(fd=0) to disable the windows before exiting
         */
        if (ctx->got_eos && ctx->got_exit)
        {
            ctx->drm_renderer->setPlane(primary_plane_index, ZERO_FD,
                    0, 0, image_w, image_h,
                    0, 0, image_w << 16, image_h << 16);

            ctx->drm_renderer->setPlane(overlay_plane_index, ZERO_FD,
                    0, 0, image_w, image_h,
                    0, 0, image_w << 16, image_h << 16);

            break;
        }

        gettimeofday(&end, NULL);
        elapsed_us = (end.tv_sec - begin.tv_sec) * 1000000 +
            (end.tv_usec - begin.tv_usec);
        if (elapsed_us < (1000000 / ctx->fps))
            usleep((1000000 / ctx->fps) - elapsed_us);
    } while (!ctx->got_error);

    /* Destroy the dumb framebuffers */
    for (uint32_t i = 0; i < 3; i++)
        ctx->drm_renderer->removeFB(ui_fb[i].fb_id);

    if (plane_index)
        free(plane_index);

    return NULL;
}

static void
nvbuf_cleanup(context_t *ctx)
{
    int index = 0;
    for (index = 0; index < ctx->numCapBuffers; index++) {
        NvBufferDestroy(ctx->dec_fd[index]);
    }
    ctx->numCapBuffers = 0;
    for (index = 0; index < ctx->numRenderBuffers; index++) {
        NvBufferDestroy(ctx->render_fd[index]);
    }
    ctx->numRenderBuffers = 0;
}

static NvBufferColorFormat
nvbuf_set_colorspace(uint32_t pixelformat,
                     uint32_t colorspace,
                     uint8_t quantization)
{
    if (pixelformat == V4L2_PIX_FMT_P010M) {
        /* 10-bit cases */
        if (colorspace == V4L2_COLORSPACE_BT2020)
            return NvBufferColorFormat_NV12_10LE_2020;
        else if (colorspace == V4L2_COLORSPACE_REC709)
            return NvBufferColorFormat_NV12_10LE_709;
        else
            return NvBufferColorFormat_NV12_10LE;
    } else {
        /* 8-bit cases */
        switch(colorspace)
        {
        case V4L2_COLORSPACE_SMPTE170M:
            if (quantization == V4L2_QUANTIZATION_DEFAULT)
            {
                cout << "Colorspace ITU-R BT.601 with standard range luma (16-235)" << endl;
                return NvBufferColorFormat_NV12;
            }
            else
            {
                cout << "Colorspace ITU-R BT.601 with extended range luma (0-255)" << endl;
                return NvBufferColorFormat_NV12_ER;
            }
            break;
        case V4L2_COLORSPACE_REC709:
            if (quantization == V4L2_QUANTIZATION_DEFAULT)
            {
                cout << "Colorspace ITU-R BT.709 with standard range luma (16-235)" << endl;
                return NvBufferColorFormat_NV12_709;
            }
            else
            {
                cout << "Colorspace ITU-R BT.709 with extended range luma (0-255)" << endl;
                return NvBufferColorFormat_NV12_709_ER;
            }
            break;
        case V4L2_COLORSPACE_BT2020:
            {
                cout << "Colorspace ITU-R BT.2020" << endl;
                return NvBufferColorFormat_NV12_2020;
            }
            break;
        default:
            cout << "Supported colorspace details not available, use default" << endl;
            if (quantization == V4L2_QUANTIZATION_DEFAULT)
            {
                cout << "Colorspace ITU-R BT.601 with standard range luma (16-235)" << endl;
                return NvBufferColorFormat_NV12;
            }
            else
            {
                cout << "Colorspace ITU-R BT.601 with extended range luma (0-255)" << endl;
                return NvBufferColorFormat_NV12_ER;
            }
            break;
        }
    }
}

static void
query_and_set_capture(context_t * ctx)
{
    NvVideoDecoder *dec = ctx->dec;
    struct v4l2_format format;
    struct v4l2_crop crop;
    int32_t min_dec_capture_buffers;
    int ret = 0;
    int error = 0;
    uint32_t window_width;
    uint32_t window_height;
    v4l2_ctrl_video_displaydata displaydata;
    v4l2_ctrl_video_hdrmasteringdisplaydata v4l2_hdrmetadata;
    struct drm_tegra_hdr_metadata_smpte_2086 drm_metadata;
    NvBufferCreateParams cParams = {0};
    int index = 0;

    memset(&v4l2_hdrmetadata,0,sizeof(v4l2_ctrl_video_hdrmasteringdisplaydata));
    memset(&drm_metadata,0,sizeof(struct drm_tegra_hdr_metadata_smpte_2086));

    /**
     * Get capture plane format from the decoder. This may change after
     * an resolution change event
     */
    ret = dec->capture_plane.getFormat(format);
    TEST_ERROR(ret < 0,
               "Error: Could not get format from decoder capture plane", error);

    /* Get the display resolution from the decoder */
    ret = dec->capture_plane.getCrop(crop);
    TEST_ERROR(ret < 0,
               "Error: Could not get crop from decoder capture plane", error);

    cout << "Video Resolution: " << crop.c.width << "x" << crop.c.height
        << endl;

    ret = dec->checkifMasteringDisplayDataPresent(displaydata);
    if (ret == 0)
    {
        if (displaydata.masteringdisplaydatapresent)
        {
            ctx->streamHDR = true;
            ret = dec->MasteringDisplayData(&v4l2_hdrmetadata);
            TEST_ERROR(ret < 0,
                    "Error while getting HDR mastering display data",
                    error);
                memcpy(&drm_metadata,&v4l2_hdrmetadata,sizeof(v4l2_ctrl_video_hdrmasteringdisplaydata));
        }
        else
            cout << "APP_INFO : mastering display data not found" << endl;
    }

    /* Destroy the old instance of renderer as resolution might have changed */
    if (ctx->drm_renderer)
        delete ctx->drm_renderer;

    nvbuf_cleanup(ctx);

    if (ctx->window_width && ctx->window_height)
    {
        /* As specified by user on commandline */
        window_width = ctx->window_width;
        window_height = ctx->window_height;
    }
    else
    {

        if (ctx->disable_ui || ctx->disable_video ||
           (crop.c.width >= 3840 && crop.c.height >= 2160)) {
            /**
             * Resolution got from the decoder
             * Not perform downscale for 4K video
             */
            window_width = crop.c.width;
            window_height = crop.c.height;
        } else {
            /**
             * If we render both UI and video stream, here it scales down
             * video stream by 2 to get a better user experience
             */
            window_width =  crop.c.width / 2;
            window_height =  crop.c.height / 2;
        }
    }

    ctx->drm_renderer = NvDrmRenderer::createDrmRenderer("renderer0",
            window_width, window_height, ctx->window_x, ctx->window_y,
             ctx->connector, ctx->crtc, drm_metadata, ctx->streamHDR);

    TEST_ERROR(!ctx->drm_renderer,
            "Error in setting up drm renderer", error);

    ctx->drm_renderer->setFPS(ctx->fps);

    /* Enable data profiling for renderer */
    if (ctx->stats)
        ctx->drm_renderer->enableProfiling();

    if (!ctx->disable_ui)
    {
        pthread_create(&ctx->ui_renderer_loop, NULL,
                ui_render_loop_fcn, ctx);
        pthread_setname_np(ctx->ui_renderer_loop,"UIRendererLoop");
    }

    /* deinitPlane unmaps the buffers and calls REQBUFS with count 0 */
    dec->capture_plane.deinitPlane();

    /**
     * Not necessary to call VIDIOC_S_FMT on decoder capture plane.
     * But decoder setCapturePlaneFormat function updates the class variables
     */
    ret = dec->setCapturePlaneFormat(format.fmt.pix_mp.pixelformat,
                                     format.fmt.pix_mp.width,
                                     format.fmt.pix_mp.height);
    TEST_ERROR(ret < 0, "Error in setting decoder capture plane format", error);

    /* Get the minimum buffers which have to be requested on the capture plane */
    ret = dec->getMinimumCapturePlaneBuffers(min_dec_capture_buffers);
    TEST_ERROR(ret < 0,
               "Error while getting value of minimum capture plane buffers",
               error);

    ctx->numCapBuffers = min_dec_capture_buffers + 5;
    cParams.colorFormat = nvbuf_set_colorspace(format.fmt.pix_mp.pixelformat,
                                               format.fmt.pix_mp.colorspace,
                                               format.fmt.pix_mp.quantization);
    cParams.width = crop.c.width;
    cParams.height = crop.c.height;
    cParams.layout = NvBufferLayout_BlockLinear;
    cParams.payloadType = NvBufferPayload_SurfArray;
    cParams.nvbuf_tag = NvBufferTag_VIDEO_DEC;
    /* Create block linear buffers for dec capture plane */
    for (index = 0; index < ctx->numCapBuffers; index++)
    {
        ret = NvBufferCreateEx(&ctx->dec_fd[index], &cParams);
        TEST_ERROR(ret < 0, "Failed to create buffers", error);
    }
    ret = dec->capture_plane.reqbufs(V4L2_MEMORY_DMABUF,ctx->numCapBuffers);
    TEST_ERROR(ret, "Error in request buffers on capture plane", error);

    ctx->numRenderBuffers = 4;
    if (!ctx->streamHDR)
        cParams.colorFormat = nvbuf_set_colorspace(format.fmt.pix_mp.pixelformat,
                                                   ctx->conv_out_colorspace,
                                                   format.fmt.pix_mp.quantization);
    cParams.width = window_width;
    cParams.height = window_height;
    cParams.layout = NvBufferLayout_Pitch;
    cParams.payloadType = NvBufferPayload_SurfArray;
    cParams.nvbuf_tag = NvBufferTag_VIDEO_DEC;
    /* Create pitch linear buffers for renderring */
    for (index = 0; index < ctx->numRenderBuffers; index++)
    {
        ret = NvBufferCreateEx(&ctx->render_fd[index], &cParams);
        TEST_ERROR(ret < 0, "Failed to create buffers", error);
    }

    /* Capture plane STREAMON */
    ret = dec->capture_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in decoder capture plane streamon", error);

    /* Enqueue all the empty capture plane buffers */
    for (uint32_t i = 0; i < dec->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf.memory = V4L2_MEMORY_DMABUF;
        v4l2_buf.m.planes[0].m.fd = ctx->dec_fd[i];

        ret = dec->capture_plane.qBuffer(v4l2_buf, NULL);
        TEST_ERROR(ret < 0, "Error Qing buffer at output plane", error);
    }
    cout << "Query and set capture successful" << endl;
    return;

error:
    if (error)
    {
        abort(ctx);
        cerr << "Error in " << __func__ << endl;
    }
}

static void *
dec_capture_loop_fcn(void *arg)
{
    context_t *ctx = (context_t *) arg;
    NvVideoDecoder *dec = ctx->dec;
    struct v4l2_event ev;
    int ret;
    NvBufferRect src_rect, dest_rect;
    NvBufferParams par;
    int dec_num = 0;
    int dec_width = 0, dec_height = 0;
    int render_fd;
    int render_width = 0, render_height = 0;

    cout << "Starting decoder capture loop thread" << endl;

    /**
     * Need to wait for the first Resolution change event, so that
     * the decoder knows the stream resolution and can allocate appropriate
     * buffers when we call REQBUFS
     */
    do
    {
        ret = dec->dqEvent(ev, 50000);
        if (ret < 0)
        {
            if (errno == EAGAIN)
            {
                cerr <<
                    "Timed out waiting for first V4L2_EVENT_RESOLUTION_CHANGE"
                    << endl;
            }
            else
            {
                cerr << "Error in dequeueing decoder event" << endl;
            }
            abort(ctx);
            break;
        }
    }
    while (ev.type != V4L2_EVENT_RESOLUTION_CHANGE);

    /* query_and_set_capture acts on the resolution change event */
    if (!ctx->got_error)
        query_and_set_capture(ctx);

    /* Exit on error or EOS which is signalled in main() */
    while (!(ctx->got_error || dec->isInError() || ctx->got_eos))
    {
        NvBuffer *dec_buffer;

        /* Check for Resolution change again */
        ret = dec->dqEvent(ev, false);
        if (ret == 0)
        {
            switch (ev.type)
            {
                case V4L2_EVENT_RESOLUTION_CHANGE:
                    query_and_set_capture(ctx);
                    dec_num = 0;
                    dec_width = dec_height = 0;
                    render_width = render_height = 0;
                    continue;
            }
        }

        while (1)
        {
            struct v4l2_buffer v4l2_buf;
            struct v4l2_plane planes[MAX_PLANES];

            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            memset(planes, 0, sizeof(planes));
            v4l2_buf.m.planes = planes;

            /* Dequeue a filled buffer */
            if (dec->capture_plane.dqBuffer(v4l2_buf, &dec_buffer, NULL, 0))
            {
                if (errno == EAGAIN)
                {
                    usleep(1000);
                }
                else
                {
                    abort(ctx);
                    cerr << "Error while calling dequeue at capture plane" <<
                        endl;
                }
                break;
            }

            dec_buffer->planes[0].fd = ctx->dec_fd[v4l2_buf.index];
            if (dec_width == 0 || dec_height == 0) {
                NvBufferGetParams (ctx->dec_fd[v4l2_buf.index], &par);
                dec_width = par.width[0];
                dec_height = par.height[0];
            }

            /**
             * Only render BT601
             * TBD: drm render to support BT709 and BT2020
             */
            if (ctx->conv_out_colorspace == V4L2_COLORSPACE_SMPTE170M)
                ctx->disable_video = false;
            else
                ctx->disable_video = true;

            if (ctx->disable_video) {
                render_fd = ctx->render_fd[0];
            } else {
                if (dec_num < ctx->numRenderBuffers) {
                    render_fd = ctx->render_fd[dec_num];
                    dec_num++;
                } else {
                    render_fd = ctx->drm_renderer->dequeBuffer();
                }
            }
            if (render_width == 0 || render_height == 0) {
                NvBufferGetParams (render_fd, &par);
                render_width = par.width[0];
                render_height = par.height[0];
            }
            /* Clip & Stitch can be done by adjusting rectangle */
            src_rect.top = 0;
            src_rect.left = 0;
            src_rect.width = dec_width;
            src_rect.height = dec_height;
            dest_rect.top = 0;
            dest_rect.left = 0;
            dest_rect.width = render_width;
            dest_rect.height = render_height;

            NvBufferTransformParams transform_params;
            memset(&transform_params, 0, sizeof(transform_params));
            /* Indicates which of the transform parameters are valid */
            transform_params.transform_flag = NVBUFFER_TRANSFORM_FILTER;
            transform_params.transform_flip = NvBufferTransform_None;
            transform_params.transform_filter = NvBufferTransform_Filter_Smart;
            transform_params.src_rect = src_rect;
            transform_params.dst_rect = dest_rect;

            /* Convert YUV block linear data to YUV pitch linear data */
            ret = NvBufferTransform(dec_buffer->planes[0].fd, render_fd, &transform_params);
            if (ret < 0)
            {
                cerr << "Transform failed" << endl;
                break;
            }
            if (ctx->out_file) {
                /* dump YUVs */
                dump_dmabuf(render_fd, 0, ctx->out_file);
                dump_dmabuf(render_fd, 1, ctx->out_file);
            }
            if (!ctx->disable_video) {
                /* Queue render_fd to renderer */
                ctx->drm_renderer->enqueBuffer(render_fd);
            }
            /* Queue dec_fd to decoder capture plance */
            v4l2_buf.m.planes[0].m.fd = ctx->dec_fd[v4l2_buf.index];
            if (dec->capture_plane.qBuffer(v4l2_buf, NULL) < 0)
            {
                abort(ctx);
                cerr <<
                    "Error while queueing buffer at decoder capture plane"
                    << endl;
                break;
            }
        }
    }

    if (!ctx->disable_video && ctx->drm_renderer)
        ctx->drm_renderer->enqueBuffer(-1);

    cout << "Exiting decoder capture loop thread" << endl;
    return NULL;
}

static void
set_defaults(context_t * ctx)
{
    ctx->dec = NULL;
    ctx->decoder_pixfmt = 1;
    ctx->in_file_path = NULL;
    ctx->in_file = NULL;
    ctx->numCapBuffers = 0;
    ctx->numRenderBuffers = 0;
    ctx->conv_out_colorspace = V4L2_COLORSPACE_SMPTE170M;
    ctx->out_file_path = NULL;
    ctx->out_file = NULL;
    ctx->drm_renderer = NULL;
    ctx->disable_video = false;
    ctx->disable_ui = false;
    ctx->console_fd = -1;
    ctx->active_vt = -1;
    ctx->crtc = 0;
    ctx->connector = 0;
    ctx->window_height = 0;
    ctx->window_width = 0;
    ctx->window_x = 0;
    ctx->window_y = 0;
    ctx->fps = 30;

    ctx->dec_capture_loop = 0;
    ctx->got_error = false;
    ctx->got_eos = false;

    ctx->ui_renderer_loop = 0;
    ctx->got_exit = false;
    ctx->streamHDR = false;

    ctx->stress_iteration = 0;
    ctx->stats = false;
}

static resolution res_array[] = {
    {1920, 1080},
    {1280, 720},
    {960, 640},
    {640, 480},
};

static int
drm_rendering(context_t &ctx, int argc, char *argv[], int iteration)
{
    int ret = 0;
    int error = 0;
    uint32_t i;
    bool eos = false;
    NvApplicationProfiler &profiler = NvApplicationProfiler::getProfilerInstance();
    struct drm_tegra_hdr_metadata_smpte_2086 metadata;

    set_defaults(&ctx);

    enter_vt(&ctx);

    pthread_setname_np(pthread_self(),"OutputPlane");

    if (parse_csv_args(&ctx, argc, argv))
    {
        fprintf(stderr, "Error parsing commandline arguments\n");
        return -1;
    }

    if (ctx.stress_iteration)
    {
        cout << "\nStart the iteration: " << iteration << "\n" <<endl;
        ctx.window_width = res_array[iteration % 4].width;
        ctx.window_height = res_array[iteration % 4].height;
    }

    if (ctx.stats)
        profiler.start(NvApplicationProfiler::DefaultSamplingInterval);

    /* Render UI infinitely until user terminate it */
    if (ctx.disable_video)
    {
        ctx.drm_renderer = NvDrmRenderer::createDrmRenderer("renderer0",
                image_w, image_h, 0, 0, ctx.connector, ctx.crtc, metadata, ctx.streamHDR);

        TEST_ERROR(!ctx.drm_renderer, "Error creating drm renderer", cleanup);

        ctx.drm_renderer->setFPS(ctx.fps);

        /* Enable data profiling for renderer */
        if (ctx.stats)
            ctx.drm_renderer->enableProfiling();

        pthread_create(&ctx.ui_renderer_loop, NULL,
                ui_render_loop_fcn, &ctx);
        pthread_setname_np(ctx.ui_renderer_loop,"UIRendererLoop");

        goto cleanup;
    }

    /**
     * Otherwise, it renders both video and UI, or render only video
     * when the option '--disable-ui' specified
     *
     * The pipelie of this case is,
     * File --> Decoder --> Converter(NV12/BL -> NV12/PL) --> DRM
     */

    /* ** Step 1 - Create video decoder ** */
    ctx.dec = NvVideoDecoder::createVideoDecoder("dec0");
    TEST_ERROR(!ctx.dec, "Could not create decoder", cleanup);

    /* Enable data profiling for decoder */
    if (ctx.stats)
        ctx.dec->enableProfiling();

    /* Subscribe to Resolution change event */
    ret = ctx.dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE, 0, 0);
    TEST_ERROR(ret < 0, "Could not subscribe to V4L2_EVENT_RESOLUTION_CHANGE",
               cleanup);

    /* Set format on the decoder output plane */
    ret = ctx.dec->setOutputPlaneFormat(ctx.decoder_pixfmt, CHUNK_SIZE);
    TEST_ERROR(ret < 0, "Could not set output plane format", cleanup);

    /**
     * Set V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT control to false
     * so that application can send chunks of encoded data instead of forming
     * complete frames.
     */
    ret = ctx.dec->setFrameInputMode(1);
    TEST_ERROR(ret < 0,
            "Error in decoder setFrameInputMode", cleanup);

    /**
     * Query, Export and Map the output plane buffers so that we can read
     * encoded data into the buffers
     */
    ret = ctx.dec->output_plane.setupPlane(V4L2_MEMORY_MMAP, 10, true, false);
    TEST_ERROR(ret < 0, "Error while setting up output plane", cleanup);

    ctx.in_file = new ifstream(ctx.in_file_path);
    TEST_ERROR(!ctx.in_file->is_open(), "Error opening input file", cleanup);

    if (ctx.out_file_path)
    {
        ctx.out_file = new ofstream(ctx.out_file_path);
        TEST_ERROR(!ctx.out_file->is_open(), "Error opening output file",
                   cleanup);
    }

    ret = ctx.dec->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in output plane stream on", cleanup);

    /* ** Step 3 - Set up decoder and converter in sub-thread ** */
    pthread_create(&ctx.dec_capture_loop, NULL, dec_capture_loop_fcn, &ctx);
    pthread_setname_np(ctx.dec_capture_loop,"CapturePlane");

    /**
     * ** Step 4 - feed the encoded data into decoder output plane **
     * Read encoded data and enqueue all the output plane buffers.
     * Exit loop in case file read is complete.
     */
    i = 0;
    while (!eos && !ctx.got_error && !ctx.dec->isInError() &&
           i < ctx.dec->output_plane.getNumBuffers())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        buffer = ctx.dec->output_plane.getNthBuffer(i);

        read_decoder_input_chunk(ctx.in_file, buffer);

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;

        /**
         * It is necessary to queue an empty buffer to signal EOS to the decoder
         * i.e. set v4l2_buf.m.planes[0].bytesused = 0 and queue the buffer
         */
        ret = ctx.dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error Qing buffer at output plane" << endl;
            abort(&ctx);
            break;
        }
        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            eos = true;
            cout << "Input file read complete" << endl;
            break;
        }
        i++;
    }

    /**
     * Since all the output plane buffers have been queued, we first need to
     * dequeue a buffer from output plane before we can read new data into it
     * and queue it again.
     */
    while (!eos && !ctx.got_error && !ctx.dec->isInError())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;

        ret = ctx.dec->output_plane.dqBuffer(v4l2_buf, &buffer, NULL, -1);
        if (ret < 0)
        {
            cerr << "Error DQing buffer at output plane" << endl;
            abort(&ctx);
            break;
        }

        read_decoder_input_chunk(ctx.in_file, buffer);

        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;
        ret = ctx.dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error Qing buffer at output plane" << endl;
            abort(&ctx);
            break;
        }
        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            eos = true;
            cout << "Input file read complete" << endl;
            break;
        }
    }

    /**
     * After sending EOS, all the buffers from output plane should be dequeued.
     * and after that capture plane loop should be signalled to stop.
     */
    while (ctx.dec->output_plane.getNumQueuedBuffers() > 0 &&
           !ctx.got_error && !ctx.dec->isInError())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;
        ret = ctx.dec->output_plane.dqBuffer(v4l2_buf, NULL, NULL, -1);
        if (ret < 0)
        {
            cerr << "Error DQing buffer at output plane" << endl;
            abort(&ctx);
            break;
        }
    }

    /* Signal EOS to the decoder capture loop */
    ctx.got_eos = true;

    if (ctx.stats)
    {
        profiler.stop();
        if (ctx.dec)
            ctx.dec->printProfilingStats(cout);
        if (ctx.drm_renderer)
            ctx.drm_renderer->printProfilingStats(cout);
        profiler.printProfilerData(cout);
    }

cleanup:
    if (ctx.dec_capture_loop)
    {
        pthread_join(ctx.dec_capture_loop, NULL);
    }

    if (ctx.ui_renderer_loop)
    {
        ctx.got_exit = true;
        pthread_join(ctx.ui_renderer_loop, NULL);
    }

    if (ctx.dec && ctx.dec->isInError())
    {
        cerr << "Decoder is in error" << endl;
        error = 1;
    }

    if (ctx.got_error)
    {
        error = 1;
    }

    leave_vt(&ctx);

    /**
     * The decoder destructor does all the cleanup i.e set streamoff on output and capture planes,
     * unmap buffers, tell decoder to deallocate buffer (reqbufs ioctl with counnt = 0),
     * and finally call v4l2_close on the fd.
     */
    delete ctx.dec;
    /* Similarly, NvDrmRenderer destructor does all the cleanup */
    delete ctx.drm_renderer;
    delete ctx.out_file;
    free(ctx.out_file_path);

    nvbuf_cleanup(&ctx);

    delete ctx.in_file;

    free(ctx.in_file_path);

    if (ctx.stress_iteration)
    {
        if (error)
            cout << "\nERROR: failed in iteration: " << iteration << endl;
        else
            cout << "\nEnd the iteration: " << iteration << "\n" <<endl;
    }

    return error;
}

int
main(int argc, char *argv[])
{
    context_t ctx;
    int ret = 0;
    uint32_t iteration = 1;

    do
    {
        /* Main loop to render UI(ARGB) or Video(YUV420) stream */
        ret = drm_rendering(ctx, argc, argv, iteration);

    } while (iteration++ < ctx.stress_iteration && ret == 0);

    if (ret)
    {
        cout << "App run failed" << endl;
    }
    else
    {
        cout << "App run was successful" << endl;
    }

    return -ret;
}
