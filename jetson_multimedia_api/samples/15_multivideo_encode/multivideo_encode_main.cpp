/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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
#include <iostream>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <linux/videodev2.h>
#include <malloc.h>
#include "nvbuf_utils.h"

#include "multivideo_encode.h"

#define TEST_ERROR(cond, str, label) if (cond) { \
                                        cerr << str << endl; \
                                        error = 1; \
                                        goto label; }

int num_files;

/**
  * Set encoder context defaults values.
  *
  * @param ctx : Encoder contexts
  */
static void
set_defaults(context_t ** ctx)
{
    for (int i = 0 ; i < num_files; i++)
    {
        ctx[i] = new context_t;
        memset (ctx[i], 0, sizeof(context_t));

        ctx[i]->thread_num = i;
        ctx[i]->raw_pixfmt = V4L2_PIX_FMT_YUV420M;
        ctx[i]->bitrate = 4 * 1024 * 1024;
        ctx[i]->peak_bitrate = 0;
        ctx[i]->profile = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
        ctx[i]->ratecontrol = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
        ctx[i]->iframe_interval = 30;
        ctx[i]->enableLossless = false;
        ctx[i]->idr_interval = 256;
        ctx[i]->level = -1;
        ctx[i]->fps_n = 30;
        ctx[i]->fps_d = 1;
        ctx[i]->stress_test = 1;
        ctx[i]->output_memory_type = V4L2_MEMORY_DMABUF;
        ctx[i]->blocking_mode = 1;
        ctx[i]->num_output_buffers = 6;
        ctx[i]->hw_preset_type = V4L2_ENC_HW_PRESET_ULTRAFAST;
    }
}

/**
  * Abort on error.
  *
  * @param ctx : Encoder context
  */
static void
abort(context_t *ctx)
{
    ctx->got_error = true;
    ctx->enc->abort();
}

/**
  * Write encoded frame data.
  *
  * @param stream : output stream
  * @param buffer : output nvbuffer
  */
static int
write_encoder_output_frame (ofstream * stream, NvBuffer * buffer)
{
    stream->write ((char *) buffer->planes[0].data, buffer->planes[0].bytesused);
    return 0;
}

/**
  * Encoder capture-plane deque buffer callback function.
  *
  * @param v4l2_buf      : v4l2 buffer
  * @param buffer        : NvBuffer
  * @param shared_buffer : shared NvBuffer
  * @param arg           : context pointer
  */
static bool
encoder_capture_plane_dq_callback(struct v4l2_buffer *v4l2_buf, NvBuffer *buffer,
                                  NvBuffer *shared_buffer, void *arg)
{
    context_t &ctx = *(context_t *) arg;
    NvVideoEncoder *enc = ctx.enc;

    char enc_cap_plane[16] = "EncCapPlane";
    string s = to_string (ctx.thread_num);
    strcat (enc_cap_plane, s.c_str());
    pthread_setname_np (pthread_self(), enc_cap_plane);

    uint32_t frame_num = ctx.enc->capture_plane.getTotalDequeuedBuffers() - 1;
    static uint32_t num_encoded_frames = 1;

    if (v4l2_buf == NULL)
    {
        cout << "Error while dequeing buffer from output plane" << endl;
        abort (&ctx);
        return false;
    }

    /* Received EOS from encoder. Stop dqthread. */
    if (buffer->planes[0].bytesused == 0)
    {
        cout << "Got 0 size buffer in capture \n";
        return false;
    }

    write_encoder_output_frame (ctx.out_file, buffer);
    num_encoded_frames++;

    if (ctx.dump_mv)
    {
        /* Get motion vector parameters of the frames from encoder */
        v4l2_ctrl_videoenc_outputbuf_metadata_MV enc_mv_metadata;
        if (ctx.enc->getMotionVectors (v4l2_buf->index, enc_mv_metadata) == 0)
        {
            uint32_t numMVs = enc_mv_metadata.bufSize / sizeof(MVInfo);
            MVInfo *pInfo = enc_mv_metadata.pMVInfo;

            *ctx.mv_dump_file << "Frame " << frame_num << ": Num MVs=" << numMVs << endl;

            for (uint32_t i = 0; i < numMVs; i++, pInfo++)
            {
                *ctx.mv_dump_file << i << ": mv_x=" << pInfo->mv_x <<
                                 " mv_y=" << pInfo->mv_y <<
                                 " weight=" << pInfo->weight <<
                                 endl;
            }
        }
    }

    /* encoder qbuffer for capture plane */
    if (enc->capture_plane.qBuffer (*v4l2_buf, NULL) < 0)
    {
        cerr << "Error while Qing buffer at capture plane" << endl;
        abort (&ctx);
        return false;
    }

    return true;
}

/**
  * Setup output plane for DMABUF io-mode.
  *
  * @param ctx         : encoder context
  * @param num_buffers : request buffer count
  */
static int
setup_output_dmabuf(context_t &ctx, uint32_t num_buffers )
{
    int ret=0;
    NvBufferCreateParams cParams;
    int fd;
    ret = ctx.enc->output_plane.reqbufs (V4L2_MEMORY_DMABUF,num_buffers);
    if (ret)
    {
        cerr << "reqbufs failed for output plane V4L2_MEMORY_DMABUF" << endl;
        return ret;
    }

    for (uint32_t i = 0; i < ctx.enc->output_plane.getNumBuffers(); i++)
    {
        cParams.width = ctx.width;
        cParams.height = ctx.height;
        cParams.layout = NvBufferLayout_Pitch;
        if (ctx.enableLossless && ctx.encoder_pixfmt == V4L2_PIX_FMT_H264)
        {
            cParams.colorFormat = NvBufferColorFormat_YUV444;
        }
        else if (ctx.profile == V4L2_MPEG_VIDEO_H265_PROFILE_MAIN10)
        {
            cParams.colorFormat = NvBufferColorFormat_NV12_10LE;
        }
        else
        {
            cParams.colorFormat = ctx.enable_extended_colorformat ?
                 NvBufferColorFormat_YUV420_ER : NvBufferColorFormat_YUV420;
        }
        cParams.nvbuf_tag = NvBufferTag_VIDEO_ENC;
        cParams.payloadType = NvBufferPayload_SurfArray;

        /* Create output plane fd for DMABUF io-mode */
        ret = NvBufferCreateEx (&fd, &cParams);
        if(ret < 0)
        {
            cerr << "Failed to create NvBuffer" << endl;
            return ret;
        }
        ctx.output_plane_fd[i] = fd;
    }
    return ret;
}

/**
  * Encoder polling thread loop function.
  *
  * @param args : void arguments
  */
static void *encoder_pollthread_fcn(void *arg)
{
    context_t &ctx = *(context_t *) arg;
    v4l2_ctrl_video_device_poll devicepoll;

    cout << "Starting Device Poll Thread" << endl;

    memset (&devicepoll, 0, sizeof(v4l2_ctrl_video_device_poll));

    /*
     * Wait here until signalled to issue the Poll call.
     * Check if the abort status is set. If so, exit.
     * Else, issue the Poll on the encoder and block.
     * When the Poll returns, signal the encoder thread to continue.
     */
    while (!ctx.got_error && !ctx.enc->isInError())
    {
        sem_wait (&ctx.pollthread_sema);

        if (ctx.got_eos)
        {
            cout << "Got eos, exiting poll thread\n";
            return NULL;
        }

        devicepoll.req_events = POLLIN | POLLOUT | POLLERR | POLLPRI;

        /* This call shall wait in the v4l2 encoder library */
        ctx.enc->DevicePoll (&devicepoll);

        /* Can check the devicepoll.resp_events bitmask to see which events are set. */
        sem_post (&ctx.encoderthread_sema);
    }

    return NULL;
}

/**
  * Encode processing function for non-blocking mode.
  *
  * @param ctx : Encoder context
  * @param eos : end of stream
  */
static int encoder_proc_nonblocking(context_t &ctx, bool eos)
{
    /*
     * NOTE: In non-blocking mode, we will have this function do below things:
     *     1) Issue signal to PollThread so it starts Poll and wait until signalled.
     *     2) After we are signalled, it means there is something to dequeue,
     *        either output plane or capture plane or there's an event.
     *     3) Try dequeuing from all three and then act appropriately.
     *     4) After enqueuing go back to the same loop.
     */

    /*
     * Since all the output plane buffers have been queued, we first need to
     * dequeue a buffer from output plane before we can read new data into it
     * and queue it again.
     */
    int ret = 0;

    while (!ctx.got_error && !ctx.enc->isInError())
    {
        /* Call SetPollInterrupt */
        ctx.enc->SetPollInterrupt();

        /* Since buffers have been queued, issue a post to start polling and
           then wait here */
        sem_post (&ctx.pollthread_sema);
        sem_wait (&ctx.encoderthread_sema);

        /* Already end of file, no more queue-dequeue for output plane */
        if (eos)
            goto check_capture_buffers;

        /* Check if can dequeue from output plane */
        while (1)
        {
            struct v4l2_buffer v4l2_output_buf;
            struct v4l2_plane output_planes[MAX_PLANES];
            NvBuffer *outplane_buffer = NULL;

            memset (&v4l2_output_buf, 0, sizeof(v4l2_output_buf));
            memset (output_planes, 0, sizeof(output_planes));
            v4l2_output_buf.m.planes = output_planes;

            /* Dequeue from output plane, fill the frame and enqueue it back again.
               NOTE: This could be moved out to a different thread as an optimization. */
            ret = ctx.enc->output_plane.dqBuffer (v4l2_output_buf,
                                                 &outplane_buffer, NULL, 10);
            if (ret < 0)
            {
                if (errno == EAGAIN)
                {
                    goto check_capture_buffers;
                }
                cerr << "ERROR while DQing buffer at output plane" << endl;
                abort (&ctx);
                return -1;
            }

            /* Read yuv frame data from input file */
            if (read_video_frame (ctx.in_file, *outplane_buffer) < 0)
            {
                cerr << "Could not read complete frame from input file" << endl;
                v4l2_output_buf.m.planes[0].bytesused = 0;

                eos = true;
                v4l2_output_buf.m.planes[0].m.userptr = 0;
                v4l2_output_buf.m.planes[0].bytesused = 0;
                v4l2_output_buf.m.planes[1].bytesused = 0;
                v4l2_output_buf.m.planes[2].bytesused = 0;
            }

            if(ctx.output_memory_type == V4L2_MEMORY_DMABUF ||
               ctx.output_memory_type == V4L2_MEMORY_MMAP)
            {
                for (uint32_t j = 0 ; j < outplane_buffer->n_planes; j++)
                {
                    ret = NvBufferMemSyncForDevice (outplane_buffer->planes[j].fd,
                                  j, (void **)&outplane_buffer->planes[j].data);
                    if (ret < 0)
                    {
                        cerr << "Error while NvBufferMemSyncForDevice at "
                                "output plane for V4L2_MEMORY_DMABUF" << endl;
                        abort (&ctx);
                        return -1;
                    }
                }
            }

            if(ctx.output_memory_type == V4L2_MEMORY_DMABUF)
            {
                for (uint32_t j = 0 ; j < outplane_buffer->n_planes; j++)
                {
                    v4l2_output_buf.m.planes[j].bytesused = outplane_buffer->planes[j].bytesused;
                }
            }

            /* encoder qbuffer for output plane */
            ret = ctx.enc->output_plane.qBuffer (v4l2_output_buf, NULL);
            if (ret < 0)
            {
                cerr << "Error while queueing buffer at output plane" << endl;
                abort (&ctx);
                return -1;
            }
            ctx.input_frames_queued_count++;
            if (v4l2_output_buf.m.planes[0].bytesused == 0)
            {
                cerr << "File read complete." << endl;
                eos = true;
                goto check_capture_buffers;
            }
        }

check_capture_buffers:
        while (1)
        {
            struct v4l2_buffer v4l2_capture_buf;
            struct v4l2_plane capture_planes[MAX_PLANES];
            NvBuffer *capplane_buffer = NULL;
            bool capture_dq_continue = true;

            memset (&v4l2_capture_buf, 0, sizeof(v4l2_capture_buf));
            memset (capture_planes, 0, sizeof(capture_planes));
            v4l2_capture_buf.m.planes = capture_planes;
            v4l2_capture_buf.length = 1;

            /* Dequeue from output plane, fill the frame and enqueue it back again.
               NOTE: This could be moved out to a different thread as an optimization. */
            ret = ctx.enc->capture_plane.dqBuffer (v4l2_capture_buf,
                                                  &capplane_buffer, NULL, 10);
            if (ret < 0)
            {
                if (errno == EAGAIN)
                    break;
                cerr << "ERROR while DQing buffer at capture plane" << endl;
                abort (&ctx);
                return -1;
            }

            /* Invoke encoder capture-plane deque buffer callback */
            capture_dq_continue = encoder_capture_plane_dq_callback
                                (&v4l2_capture_buf, capplane_buffer, NULL, &ctx);
            if (!capture_dq_continue)
            {
                cout << "Capture plane dequeued 0 size buffer " << endl;
                ctx.got_eos = true;
                return 0;
            }
        }
    }

    return 0;
}

/**
  * Encode processing function for blocking mode.
  *
  * @param ctx : Encoder context
  * @param eos : end of stream
  */
static int encoder_proc_blocking(context_t &ctx, bool eos)
{
    int ret = 0;
    /* Keep reading input till EOS is reached */
    while (!ctx.got_error && !ctx.enc->isInError() && !eos)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        memset (&v4l2_buf, 0, sizeof(v4l2_buf));
        memset (planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;

        /* Dequeue buffer from encoder output plane */
        if (ctx.enc->output_plane.dqBuffer (v4l2_buf, &buffer, NULL, 10) < 0)
        {
            cerr << "ERROR while DQing buffer at output plane" << endl;
            abort (&ctx);
            return -1;
        }

        /* Read yuv frame data from input file */
        if (read_video_frame (ctx.in_file, *buffer) < 0)
        {
            cerr << "Could not read complete frame from input file" << endl;
            v4l2_buf.m.planes[0].bytesused = 0;

            eos = true;
            ctx.got_eos = true;
            v4l2_buf.m.planes[0].m.userptr = 0;
            v4l2_buf.m.planes[0].bytesused = v4l2_buf.m.planes[1].bytesused
                                           = v4l2_buf.m.planes[2].bytesused = 0;
        }

        if (ctx.output_memory_type == V4L2_MEMORY_DMABUF ||
            ctx.output_memory_type == V4L2_MEMORY_MMAP)
        {
            for (uint32_t j = 0; j < buffer->n_planes; j++)
            {
                ret = NvBufferMemSyncForDevice (buffer->planes[j].fd, j,
                                                (void **)&buffer->planes[j].data);
                if (ret < 0)
                {
                    cerr << "Error while NvBufferMemSyncForDevice at "
                            "output plane for V4L2_MEMORY_DMABUF" << endl;
                    abort (&ctx);
                    return -1;
                }
            }
        }

        if(ctx.output_memory_type == V4L2_MEMORY_DMABUF)
        {
            for (uint32_t j = 0 ; j < buffer->n_planes ; j++)
            {
                v4l2_buf.m.planes[j].bytesused = buffer->planes[j].bytesused;
            }
        }

        /* encoder qbuffer for output plane */
        ret = ctx.enc->output_plane.qBuffer (v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error while queueing buffer at output plane" << endl;
            abort (&ctx);
            return -1;
        }
        ctx.input_frames_queued_count++;
        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            cerr << "File read complete." << endl;
            eos = true;
            ctx.got_eos = true;
            return 0;
        }
    }

    return -1;
}

static void *
encode_proc(void *p_ctx)
{
    context_t &ctx = *(context_t *)p_ctx;
    int ret = 0;
    int error = 0;
    bool eos = false;
    int *p_error = (int*) malloc (sizeof(int));

    if (ctx.encoder_pixfmt == V4L2_PIX_FMT_H265)
    {
        TEST_ERROR (ctx.width < 144 || ctx.height < 144, "Height/Width should be"
                    " > 144 for H.265", cleanup);
    }

    /* Open input file for raw yuv */
    ctx.in_file = new ifstream (ctx.in_file_path);
    TEST_ERROR (!ctx.in_file->is_open(), "Could not open input file", cleanup);

    /* Open output file for encoded bitstream */
    ctx.out_file = new ofstream (ctx.out_file_path);
    TEST_ERROR (!ctx.out_file->is_open(), "Couls not open output file", cleanup);

    /* Create NvVideoEncoder object for blocking or non-blocking I/O mode. */
    if (ctx.blocking_mode)
    {
        cout << "Creating Encoder in blocking mode\n";
        ctx.enc = NvVideoEncoder::createVideoEncoder ("enc0");
    }
    else
    {
        cout << "Creating Encoder in non-blocking mode\n";
        ctx.enc = NvVideoEncoder::createVideoEncoder ("enc0", O_NONBLOCK);
    }
    TEST_ERROR (!ctx.enc, "Could not create encoder", cleanup);

    /*
     * Set encoder capture plane format.
     * NOTE: It is necessary that Capture Plane format be set before Output Plane
     * format. It is necessary to set width and height on the capture plane as well
     */

    /* Set encoder capture plane format.
     * NOTE: It is necessary that Capture Plane format be set before Output Plane
     * format. It is necessary to set width and height on the capture plane as well
     */
    ret = ctx.enc->setCapturePlaneFormat (ctx.encoder_pixfmt, ctx.width,
                                         ctx.height, 2 * 1024 * 1024);
    TEST_ERROR (ret < 0, "Could not set capture plane format", cleanup);

    switch (ctx.profile)
    {
        case V4L2_MPEG_VIDEO_H265_PROFILE_MAIN10:
            ctx.raw_pixfmt = V4L2_PIX_FMT_P010M;
            break;
        case V4L2_MPEG_VIDEO_H265_PROFILE_MAIN:
        default:
            ctx.raw_pixfmt = V4L2_PIX_FMT_YUV420M;
    }

    /* Set encoder output plane format */
    if (ctx.enableLossless && ctx.encoder_pixfmt == V4L2_PIX_FMT_H264)
    {
        ctx.profile = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_444_PREDICTIVE;
        ctx.raw_pixfmt = V4L2_PIX_FMT_YUV444M;
    }
    ret = ctx.enc->setOutputPlaneFormat (ctx.raw_pixfmt, ctx.width,
                                        ctx.height);
    TEST_ERROR (ret < 0, "Could not set output plane format", cleanup);

    ret = ctx.enc->setBitrate (ctx.bitrate);
    TEST_ERROR (ret < 0, "Could not set encoder bitrate", cleanup);

    if (ctx.encoder_pixfmt == V4L2_PIX_FMT_H264)
    {
        /* Set encoder profile for H264 format */
        ret = ctx.enc->setProfile (ctx.profile);
        TEST_ERROR (ret < 0, "Could not set encoder profile", cleanup);

        if (ctx.level == (uint32_t)-1)
        {
            ctx.level = (uint32_t)V4L2_MPEG_VIDEO_H264_LEVEL_5_1;
        }

        /* Set encoder level for H264 format */
        ret = ctx.enc->setLevel (ctx.level);
        TEST_ERROR (ret < 0, "Could not set encoder level", cleanup)
    }
    else if (ctx.encoder_pixfmt == V4L2_PIX_FMT_H265)
    {
        /* Set encoder profile for HEVC format */
        ret = ctx.enc->setProfile (ctx.profile);
        TEST_ERROR (ret < 0, "Could not set encoder profile", cleanup);

        if (ctx.level != (uint32_t)-1)
        {
            /* Set encoder level for HEVC format */
            ret = ctx.enc->setLevel (ctx.level);
            TEST_ERROR (ret < 0, "Could not set encoder level", cleanup);
        }
    }

    if (ctx.enableLossless)
    {
        /* Set constant qp configuration for lossless encoding enabled */
        ret = ctx.enc->setConstantQp (0);
        TEST_ERROR (ret < 0, "Could not set encoder constant qp=0", cleanup);
    }
    else
    {
        /* Set rate control mode for encoder */
        ret = ctx.enc->setRateControlMode (ctx.ratecontrol);
        TEST_ERROR (ret < 0, "Could not set encoder rate control mode", cleanup);
        if (ctx.ratecontrol == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR)
        {
            uint32_t peak_bitrate;
            if (ctx.peak_bitrate < ctx.bitrate)
                peak_bitrate = 1.2f * ctx.bitrate;
            else
                peak_bitrate = ctx.peak_bitrate;
            /* Set peak bitrate value for variable bitrate mode for encoder */
            ret = ctx.enc->setPeakBitrate (peak_bitrate);
            TEST_ERROR (ret < 0, "Could not set encoder peak bitrate", cleanup);
        }
    }

    /* Set IDR frame interval for encoder */
    ret = ctx.enc->setIDRInterval (ctx.idr_interval);
    TEST_ERROR (ret < 0, "Could not set encoder IDR interval", cleanup);

    /* Set I frame interval for encoder */
    ret = ctx.enc->setIFrameInterval (ctx.iframe_interval);
    TEST_ERROR (ret < 0, "Could not set encoder I-Frame interval", cleanup);

    /* Set framerate for encoder */
    ret = ctx.enc->setFrameRate (ctx.fps_n, ctx.fps_d);
    TEST_ERROR (ret < 0, "Could not set framerate", cleanup);

    /* Set hardware preset value for encoder */
    ret = ctx.enc->setHWPresetType(ctx.hw_preset_type);
    TEST_ERROR(ret < 0, "Could not set encoder HW Preset Type", cleanup);

    if (ctx.num_reference_frames)
    {
        /* Set number of reference frame configuration value for encoder */
        ret = ctx.enc->setNumReferenceFrames (ctx.num_reference_frames);
        TEST_ERROR (ret < 0, "Could not set num reference frames", cleanup);
    }

    if (ctx.insert_vui)
    {
        /* Enable insert of VUI parameters */
        ret = ctx.enc->setInsertVuiEnabled (true);
        TEST_ERROR (ret < 0, "Could not set insertVUI", cleanup);
    }

    if (ctx.enable_extended_colorformat)
    {
        /* Enable extnded colorformat for encoder */
        ret = ctx.enc->setExtendedColorFormat (true);
        TEST_ERROR (ret < 0, "Could not set extended color format", cleanup);
    }

    if (ctx.dump_mv)
    {
        /* Enable dumping of motion vectors report from encoder */
        ctx.mv_dump_file = new ofstream (ctx.out_file_path + "_mvdump");
        ret = ctx.enc->enableMotionVectorReporting();
        TEST_ERROR (ret < 0, "Could not enable motion vector reporting", cleanup);
    }

    /* Query, Export and Map the output plane buffers so that we can read
     * raw data into the buffers
     */
    switch (ctx.output_memory_type)
    {
        case V4L2_MEMORY_MMAP:
            ret = ctx.enc->output_plane.setupPlane (V4L2_MEMORY_MMAP, 10, true, false);
            TEST_ERROR (ret < 0, "Could not setup output plane", cleanup);
            break;

        case V4L2_MEMORY_USERPTR:
            ret = ctx.enc->output_plane.setupPlane (V4L2_MEMORY_USERPTR, 10, false, true);
            TEST_ERROR (ret < 0, "Could not setup output plane", cleanup);
            break;

        case V4L2_MEMORY_DMABUF:
            ret = setup_output_dmabuf (ctx, 10);
            TEST_ERROR (ret < 0, "Could not setup plane", cleanup);
            break;
        default :
            TEST_ERROR (true, "Not a valid plane", cleanup);
    }

    /* Query, Export and Map the capture plane buffers so that we can write
     * encoded bitstream data into the buffers
     */
    ret = ctx.enc->capture_plane.setupPlane (V4L2_MEMORY_MMAP,
                                            ctx.num_output_buffers,
                                            true, false);
    TEST_ERROR (ret < 0, "Could not setup capture plane", cleanup);

    /* Subscibe for End Of Stream event */
    ret = ctx.enc->subscribeEvent (V4L2_EVENT_EOS, 0, 0);
    TEST_ERROR (ret < 0, "Could not subscribe EOS event", cleanup)

    /* set encoder output plane STREAMON */
    ret = ctx.enc->output_plane.setStreamStatus (true);
    TEST_ERROR (ret < 0, "Error in output plane streamon", cleanup);

    /* set encoder capture plane STREAMON */
    ret = ctx.enc->capture_plane.setStreamStatus (true);
    TEST_ERROR (ret < 0, "Error in capture plane streamon", cleanup);

    if (ctx.blocking_mode)
    {
        /* Set encoder capture plane dq thread callback for blocking io mode */
        ctx.enc->capture_plane.
            setDQThreadCallback (encoder_capture_plane_dq_callback);

        /* startDQThread starts a thread internally which calls the
         * encoder_capture_plane_dq_callback whenever a buffer is dequeued
         * on the plane
         */
        ctx.enc->capture_plane.startDQThread (&ctx);
    }
    else
    {
        sem_init (&ctx.pollthread_sema, 0, 0);
        sem_init (&ctx.encoderthread_sema, 0, 0);
        /* Set encoder poll thread for non-blocking io mode */
        pthread_create (&ctx.enc_pollthread, NULL, encoder_pollthread_fcn, &ctx);

        char enc_poll[16] = "PollThread";
        string s = to_string (ctx.thread_num);
        strcat (enc_poll, s.c_str());
        pthread_setname_np (ctx.enc_pollthread, enc_poll);
        cout << "Created the PollThread and Encoder Thread \n";
    }

    /* Enqueue all the empty capture plane buffers. */
    for (uint32_t i = 0; i < ctx.enc->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset (&v4l2_buf, 0, sizeof(v4l2_buf));
        memset (planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        ret = ctx.enc->capture_plane.qBuffer (v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error while queueing buffer at capture plane" << endl;
            abort (&ctx);
            goto cleanup;
        }
    }

    /* Read video frame and queue all the output plane buffers. */
    for (uint32_t i = 0; i < ctx.enc->output_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer = ctx.enc->output_plane.getNthBuffer (i);

        memset (&v4l2_buf, 0, sizeof(v4l2_buf));
        memset (planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        if(ctx.output_memory_type == V4L2_MEMORY_DMABUF)
        {
            v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            v4l2_buf.memory = V4L2_MEMORY_DMABUF;
            /* Map output plane buffer for memory type DMABUF. */
            ret = ctx.enc->output_plane.mapOutputBuffers (v4l2_buf, ctx.output_plane_fd[i]);

            if (ret < 0)
            {
                cerr << "Error while mapping buffer at output plane" << endl;
                abort (&ctx);
                goto cleanup;
            }
        }

        /* Read yuv frame data from input file */
        if (read_video_frame (ctx.in_file, *buffer) < 0)
        {
            cerr << "Could not read complete frame from input file" << endl;
            v4l2_buf.m.planes[0].bytesused = 0;

            eos = true;
            v4l2_buf.m.planes[0].m.userptr = 0;
            v4l2_buf.m.planes[0].bytesused = v4l2_buf.m.planes[1].bytesused
                                           = v4l2_buf.m.planes[2].bytesused = 0;
        }

        if (ctx.output_memory_type == V4L2_MEMORY_DMABUF ||
            ctx.output_memory_type == V4L2_MEMORY_MMAP)
        {
            for (uint32_t j = 0 ; j < buffer->n_planes; j++)
            {
                ret = NvBufferMemSyncForDevice (buffer->planes[j].fd, j,
                                            (void **)&buffer->planes[j].data);
                if (ret < 0)
                {
                    cerr << "Error while NvBufferMemSyncForDevice at "
                            "output plane for V4L2_MEMORY_DMABUF" << endl;
                    abort (&ctx);
                    goto cleanup;
                }
            }
        }

        if (ctx.output_memory_type == V4L2_MEMORY_DMABUF)
        {
            for (uint32_t j = 0 ; j < buffer->n_planes ; j++)
            {
                v4l2_buf.m.planes[j].bytesused = buffer->planes[j].bytesused;
            }
        }
        /* encoder qbuffer for output plane */
        ret = ctx.enc->output_plane.qBuffer (v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error while queueing buffer at output plane" << endl;
            abort (&ctx);
            goto cleanup;
        }
        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            cerr << "File read complete." << endl;
            eos = true;
            break;
        }
        ctx.input_frames_queued_count++;
    }

    if (ctx.blocking_mode)
    {
        /* Wait till capture plane DQ Thread finishes
         * i.e. all the capture plane buffers are dequeued.
         */
        if (encoder_proc_blocking (ctx, eos) != 0)
            goto cleanup;
        ctx.enc->capture_plane.waitForDQThread (-1);
    }
    else
    {
        if (encoder_proc_nonblocking (ctx, eos) != 0)
            goto cleanup;
    }

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

    if(ctx.output_memory_type == V4L2_MEMORY_DMABUF)
    {
        for (uint32_t i = 0; i < ctx.enc->output_plane.getNumBuffers(); i++)
        {
            /* Unmap output plane buffer for memory type DMABUF. */
            ret = ctx.enc->output_plane.unmapOutputBuffers (i, ctx.output_plane_fd[i]);
            if (ret < 0)
            {
                cerr << "Error while unmapping buffer at output plane" << endl;
                goto cleanup;
            }

            ret = NvBufferDestroy (ctx.output_plane_fd[i]);
            if (ret < 0)
            {
                cerr << "Failed to Destroy NvBuffer\n" << endl;
                error = 1;
                break;
            }
        }
    }

    /* Release encoder configuration specific resources. */
    if (ctx.dump_mv && ctx.mv_dump_file)
        delete ctx.mv_dump_file;

    delete ctx.enc;
    delete ctx.in_file;
    delete ctx.out_file;

    if (!ctx.blocking_mode)
    {
        sem_destroy (&ctx.pollthread_sema);
        sem_destroy (&ctx.encoderthread_sema);
    }

    if (error == 0)
    {
        cout << "Instance " << ctx.thread_num << " executed successfully.\n";
    }
    else
    {
        cout << "Instance " << ctx.thread_num << " Failed\n";
    }

    *p_error = -error;
    return (p_error);
}

/**
  * Start of video Encode application.
  *
  * @param argc : Argument Count
  * @param argv : Argument Vector
  */
int
main(int argc, char *argv[])
{
    /* create encoder contexts. */
    context_t **ctx = NULL;
    int ret = 0;
    /* save encode iterator number */
    int iterator_num = 0;
    int stress;
    void *error;

    /* Get number of encoding streams */
    num_files = get_num_files (argc, argv);
    if (num_files == -1)
    {
        cerr << "Error parsing commandline arguments\n";
        ret = -1;
        goto cleanup;
    }

    ctx = new context_t* [num_files];

    argv += 2;

    do
    {
        /* set defaults for contexts */
        set_defaults (ctx);

         /* parse the arguments */
        if (parse_csv_args (ctx, argc-3, argv, num_files))
        {
            cerr << "Error parsing commandline arguments\n";
            ret = -1;
            goto cleanup;
        }

        stress = ctx[0]->stress_test;

        for (int i = 0; i < num_files; i++)
        {
            /* Spawn multiple encoding threads for multiple encoders */
            pthread_create (&(ctx[i]->encode_thread), NULL, encode_proc, ctx[i]);
            char enc_output_plane[16] = "EncOutplane";
            string s = to_string (i);
            strcat (enc_output_plane, s.c_str());
            /* Name each spawned thread. */
            pthread_setname_np (ctx[i]->encode_thread, enc_output_plane);
        }

        for (int i = 0; i < num_files; i++)
        {
            /* Wait for the encoding thread to finish */
            pthread_join (ctx[i]->encode_thread, &error);
            if (*(int*)error != 0)
            {
                ret = *(int*)error;
            }
            free (error);
            delete ctx[i];
        }

        iterator_num++;
    } while (stress != iterator_num);

cleanup:
    if (ctx)
        delete[] ctx;

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