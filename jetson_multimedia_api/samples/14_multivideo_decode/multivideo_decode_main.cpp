/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
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

#include "NvApplicationProfiler.h"
#include "NvUtils.h"
#include <errno.h>
#include <fstream>
#include <iostream>
#include <linux/videodev2.h>
#include <malloc.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "multivideo_decode.h"
#include "nvbuf_utils.h"

#define TEST_ERROR(cond, str, label) if(cond) { \
                                        cerr << str << endl; \
                                        error = 1; \
                                        goto label; }

#define MICROSECOND_UNIT 1000000
#define CHUNK_SIZE 4000000
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#define IS_NAL_UNIT_START(buffer_ptr) (!buffer_ptr[0] && !buffer_ptr[1] && \
        !buffer_ptr[2] && (buffer_ptr[3] == 1))

#define IS_NAL_UNIT_START1(buffer_ptr) (!buffer_ptr[0] && !buffer_ptr[1] && \
        (buffer_ptr[2] == 1))

#define H264_NAL_UNIT_CODED_SLICE  1
#define H264_NAL_UNIT_CODED_SLICE_IDR  5

#define HEVC_NUT_TRAIL_N  0
#define HEVC_NUT_RASL_R  9
#define HEVC_NUT_BLA_W_LP  16
#define HEVC_NUT_CRA_NUT  21

#define IVF_FILE_HDR_SIZE   32
#define IVF_FRAME_HDR_SIZE  12

#define MAX_STREAM 32

#define IS_H264_NAL_CODED_SLICE(buffer_ptr) \
        ((buffer_ptr[0] & 0x1F) == H264_NAL_UNIT_CODED_SLICE)
#define IS_H264_NAL_CODED_SLICE_IDR(buffer_ptr) \
        ((buffer_ptr[0] & 0x1F) == H264_NAL_UNIT_CODED_SLICE_IDR)

#define GET_H265_NAL_UNIT_TYPE(buffer_ptr) ((buffer_ptr[0] & 0x7E) >> 1)

#define IS_SEMIPLANAR_FMT(pixel_format) ((pixel_format == NvBufferColorFormat_NV12) || \
        (pixel_format == NvBufferColorFormat_NV12_ER) || \
        (pixel_format == NvBufferColorFormat_NV12_709) || \
        (pixel_format == NvBufferColorFormat_NV12_709_ER) || \
        (pixel_format == NvBufferColorFormat_NV12_2020))

int num_files;
fps_stats **stream_stats;

using namespace std;

static void
print_stats()
{
    for ( int i = 0 ; i < num_files ; i++ )
    {
        cout << "*****************************************" << endl;
        cout << "Stream = " << stream_stats[i]->filename << endl;
        cout << "Total Profiling time = " <<
            (stream_stats[i]->data.profiling_time.tv_sec +
                (stream_stats[i]->data.profiling_time.tv_usec / 1000000.0))
                << endl;
        cout << "Average FPS = " << stream_stats[i]->data.average_fps << endl;
        cout << "Average latency(usec) = " <<
            stream_stats[i]->data.average_latency_usec << endl;
        cout << "Minimum latency(usec) = " <<
            stream_stats[i]->data.min_latency_usec << endl;
        cout << "Maximum latency(usec) = " <<
            stream_stats[i]->data.max_latency_usec << endl;
        cout << "*****************************************" << endl;
    }
}

/**
  * Read the input NAL unit for h264/H265/Mpeg2/Mpeg4 decoder.
  *
  * @param stream            : Input stream
  * @param buffer            : NvBuffer pointer
  * @param parse_buffer      : parse buffer pointer
  * @param parse_buffer_size : chunk size
  * @param ctx               : Decoder context
  */
static int
read_decoder_input_nalu(ifstream * stream, NvBuffer * buffer,
        char *parse_buffer, streamsize parse_buffer_size, context_t * ctx)
{
    /* Length is the size of the buffer in bytes */
    char *buffer_ptr = (char *) buffer->planes[0].data;
    int h265_nal_unit_type;
    char *stream_ptr;
    bool nalu_found = false;

    streamsize bytes_read;
    streamsize stream_initial_pos = stream->tellg();

    stream->read(parse_buffer, parse_buffer_size);
    bytes_read = stream->gcount();

    if (bytes_read == 0)
    {
        return buffer->planes[0].bytesused = 0;
    }

    /* Find the first NAL unit in the buffer */
    stream_ptr = parse_buffer;
    while ((stream_ptr - parse_buffer) < (bytes_read - 3))
    {
        nalu_found = IS_NAL_UNIT_START(stream_ptr) ||
                    IS_NAL_UNIT_START1(stream_ptr);
        if (nalu_found)
        {
            break;
        }
        stream_ptr++;
    }

    /* Reached end of buffer but could not find NAL unit */
    if (!nalu_found)
    {
        cerr << "Could not read nal unit from file. EOF or file corrupted"
            << endl;
        return -1;
    }

    memcpy(buffer_ptr, stream_ptr, 4);
    buffer_ptr += 4;
    buffer->planes[0].bytesused = 4;
    stream_ptr += 4;

    if (ctx->copy_timestamp)
    {
      if (ctx->decoder_pixfmt == V4L2_PIX_FMT_H264) {
        if ((IS_H264_NAL_CODED_SLICE(stream_ptr)) ||
            (IS_H264_NAL_CODED_SLICE_IDR(stream_ptr)))
          ctx->flag_copyts = true;
        else
          ctx->flag_copyts = false;
      } else if (ctx->decoder_pixfmt == V4L2_PIX_FMT_H265) {
        h265_nal_unit_type = GET_H265_NAL_UNIT_TYPE(stream_ptr);
        if ((h265_nal_unit_type >= HEVC_NUT_TRAIL_N &&
                h265_nal_unit_type <= HEVC_NUT_RASL_R) ||
            (h265_nal_unit_type >= HEVC_NUT_BLA_W_LP &&
             h265_nal_unit_type <= HEVC_NUT_CRA_NUT))
          ctx->flag_copyts = true;
        else
          ctx->flag_copyts = false;
      }
    }

    /* Copy bytes till the next NAL unit is found */
    while ((stream_ptr - parse_buffer) < (bytes_read - 3))
    {
        if (IS_NAL_UNIT_START(stream_ptr) || IS_NAL_UNIT_START1(stream_ptr))
        {
            streamsize seekto = stream_initial_pos +
                    (stream_ptr - parse_buffer);
            if(stream->eof())
            {
                stream->clear();
            }
            stream->seekg(seekto, stream->beg);
            return 0;
        }
        *buffer_ptr = *stream_ptr;
        buffer_ptr++;
        stream_ptr++;
        buffer->planes[0].bytesused++;
    }

    /* Reached end of buffer but could not find NAL unit */
    cerr << "Could not read nal unit from file. EOF or file corrupted"
            << endl;
    return -1;
}


/**
  * Read the input chunks for h264/H265/Mpeg2/Mpeg4 decoder.
  *
  * @param stream : Input stream
  * @param buffer : NvBuffer pointer
  */
static int
read_decoder_input_chunk(ifstream * stream, NvBuffer * buffer)
{
    /* Length is the size of the buffer in bytes */
    streamsize bytes_to_read = MIN(CHUNK_SIZE, buffer->planes[0].length);

    stream->read((char *) buffer->planes[0].data, bytes_to_read);
    /* NOTE: It is necessary to set bytesused properly, so that decoder knows how
     * many bytes in the buffer are valid */
    buffer->planes[0].bytesused = stream->gcount();
    if(buffer->planes[0].bytesused == 0)
    {
        stream->clear();
        stream->seekg(0,stream->beg);
    }
    return 0;
}

/**
  * Read the input chunks for Vp8/Vp9 decoder.
  *
  * @param ctx    : Decoder context
  * @param buffer : NvBuffer pointer
  */
static int
read_vpx_decoder_input_chunk(context_t *ctx, NvBuffer * buffer)
{
    ifstream *stream = ctx->in_file;
    int Framesize;
    unsigned char *bitstreambuffer = (unsigned char *)buffer->planes[0].data;
    if (ctx->vp9_file_header_flag == 0)
    {
        stream->read((char *) buffer->planes[0].data, IVF_FILE_HDR_SIZE);
        if (stream->gcount() !=  IVF_FILE_HDR_SIZE)
        {
            cerr << "Couldn't read IVF FILE HEADER" << endl;
            return -1;
        }
        if (!((bitstreambuffer[0] == 'D') && (bitstreambuffer[1] == 'K') &&
                    (bitstreambuffer[2] == 'I') && (bitstreambuffer[3] == 'F')))
        {
            cerr << "It's not a valid IVF file \n" << endl;
            return -1;
        }
        cout << "It's a valid IVF file" << endl;
        ctx->vp9_file_header_flag = 1;
    }
    stream->read((char *) buffer->planes[0].data, IVF_FRAME_HDR_SIZE);
    if (!stream->gcount())
    {
        cout << "End of stream" << endl;
        return 0;
    }
    if (stream->gcount() != IVF_FRAME_HDR_SIZE)
    {
        cerr << "Couldn't read IVF FRAME HEADER" << endl;
        return -1;
    }
    Framesize = (bitstreambuffer[3]<<24) + (bitstreambuffer[2]<<16) +
        (bitstreambuffer[1]<<8) + bitstreambuffer[0];
    buffer->planes[0].bytesused = Framesize;
    stream->read((char *) buffer->planes[0].data, Framesize);
    if (stream->gcount() != Framesize)
    {
        cerr << "Couldn't read Framesize" << endl;
        return -1;
    }
    return 0;
}

/**
  * Exit on error.
  *
  * @param ctx : Decoder context
  */

static void
abort(context_t *ctx)
{
    ctx->got_error = true;
    ctx->dec->abort();
#ifndef USE_NVBUF_TRANSFORM_API
    if (ctx->conv)
    {
        ctx->conv->abort();
        pthread_cond_broadcast(&ctx->queue_cond);
    }
#endif
}

#ifndef USE_NVBUF_TRANSFORM_API
static bool
conv0_output_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                   NvBuffer * buffer, NvBuffer * shared_buffer,
                                   void *arg)
{
    context_t *ctx = (context_t *) arg;
    struct v4l2_buffer dec_capture_ret_buffer;
    struct v4l2_plane planes[MAX_PLANES];

    if (!v4l2_buf)
    {
        cerr << "Error while dequeueing conv output plane buffer" << endl;
        abort(ctx);
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0)
    {
        return false;
    }

    memset(&dec_capture_ret_buffer, 0, sizeof(dec_capture_ret_buffer));
    memset(planes, 0, sizeof(planes));

    dec_capture_ret_buffer.index = shared_buffer->index;
    dec_capture_ret_buffer.m.planes = planes;
    if (ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
        dec_capture_ret_buffer.m.planes[0].m.fd =
            ctx->dmabuff_fd[shared_buffer->index];

    pthread_mutex_lock(&ctx->queue_lock);
    ctx->conv_output_plane_buf_queue->push(buffer);

    /* Return the buffer dequeued from converter output plane
       back to decoder capture plane. */
    if (ctx->dec->capture_plane.qBuffer(dec_capture_ret_buffer, NULL) < 0)
    {
        abort(ctx);
        return false;
    }

    pthread_cond_broadcast(&ctx->queue_cond);
    pthread_mutex_unlock(&ctx->queue_lock);

    return true;
}

/**
  * converter capture-plane deque buffer callback function.
  *
  * @param v4l2_buf      : v4l2 buffer
  * @param buffer        : NvBuffer
  * @param shared_buffer : shared NvBuffer
  * @param arg           : context pointer
  */
static bool
conv0_capture_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                    NvBuffer * buffer, NvBuffer * shared_buffer,
                                    void *arg)
{
    context_t *ctx = (context_t *) arg;

    if (!v4l2_buf)
    {
        cerr << "Error while dequeueing conv capture plane buffer" << endl;
        abort(ctx);
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0)
    {
        return false;
    }

    /* Write raw video frame to file. */
    if (!ctx->stats && ctx->out_file)
    {
        write_video_frame(ctx->out_file, *buffer);
    }

    if (!ctx->stats && !ctx->disable_rendering)
    {
        ctx->renderer->render(buffer->planes[0].fd);
    }
    /* Return the buffer to converter capture plane. */
    if (ctx->conv->capture_plane.qBuffer(*v4l2_buf, NULL) < 0)
    {
        return false;
    }
    return true;
}
#endif

/**
  * Report decoder input header error metadata.
  *
  * @param ctx             : Decoder context
  * @param input_metadata  : Pointer to decoder input header error metadata struct
  */
static int
report_input_metadata(context_t *ctx, v4l2_ctrl_videodec_inputbuf_metadata *input_metadata)
{
    int ret = -1;
    uint32_t frame_num = ctx->dec->output_plane.getTotalDequeuedBuffers() - 1;

    /* NOTE: Bits represent types of error as defined with v4l2_videodec_input_error_type. */
    if (input_metadata->nBitStreamError & V4L2_DEC_ERROR_SPS) {
      cout << "Frame " << frame_num << " BitStreamError : ERROR_SPS " << endl;
    } else if (input_metadata->nBitStreamError & V4L2_DEC_ERROR_PPS) {
      cout << "Frame " << frame_num << " BitStreamError : ERROR_PPS " << endl;
    } else if (input_metadata->nBitStreamError & V4L2_DEC_ERROR_SLICE_HDR) {
      cout << "Frame " << frame_num << " BitStreamError : ERROR_SLICE_HDR " << endl;
    } else if (input_metadata->nBitStreamError & V4L2_DEC_ERROR_MISSING_REF_FRAME) {
      cout << "Frame " << frame_num << " BitStreamError : ERROR_MISSING_REF_FRAME " << endl;
    } else if (input_metadata->nBitStreamError & V4L2_DEC_ERROR_VPS) {
      cout << "Frame " << frame_num << " BitStreamError : ERROR_VPS " << endl;
    } else {
      cout << "Frame " << frame_num << " BitStreamError : ERROR_None " << endl;
      ret = 0;
    }
    return ret;
}

/**
  * Report decoder output metadata.
  *
  * @param ctx      : Decoder context
  * @param metadata : Pointer to decoder output metadata struct
  */
static void
report_metadata(context_t *ctx, v4l2_ctrl_videodec_outputbuf_metadata *metadata)
{
    uint32_t frame_num = ctx->dec->capture_plane.getTotalDequeuedBuffers() - 1;

    cout << "Frame " << frame_num << endl;

    if (metadata->bValidFrameStatus)
    {
        if (ctx->decoder_pixfmt == V4L2_PIX_FMT_H264)
        {
            /* metadata for H264 input stream. */
            switch(metadata->CodecParams.H264DecParams.FrameType)
            {
                case 0:
                    cout << "FrameType = B" << endl;
                    break;
                case 1:
                    cout << "FrameType = P" << endl;
                    break;
                case 2:
                    cout << "FrameType = I";
                    if (metadata->CodecParams.H264DecParams.dpbInfo.currentFrame.bIdrFrame)
                    {
                        cout << " (IDR)";
                    }
                    cout << endl;
                    break;
            }
            cout << "nActiveRefFrames = " <<
                metadata->CodecParams.H264DecParams.dpbInfo.nActiveRefFrames << endl;
        }

        if (ctx->decoder_pixfmt == V4L2_PIX_FMT_H265)
        {
            /* metadata for HEVC input stream. */
            switch(metadata->CodecParams.HEVCDecParams.FrameType)
            {
                case 0:
                    cout << "FrameType = B" << endl;
                    break;
                case 1:
                    cout << "FrameType = P" << endl;
                    break;
                case 2:
                    cout << "FrameType = I";
                    if (metadata->CodecParams.HEVCDecParams.dpbInfo.currentFrame.bIdrFrame)
                    {
                        cout << " (IDR)";
                    }
                    cout << endl;
                    break;
            }
            cout << "nActiveRefFrames = " <<
                    metadata->CodecParams.HEVCDecParams.dpbInfo.nActiveRefFrames
                    << endl;
        }

        if (metadata->FrameDecStats.DecodeError)
        {
            /* decoder error status metadata. */
            v4l2_ctrl_videodec_statusmetadata *dec_stats =
                &metadata->FrameDecStats;
            cout << "ErrorType=" << dec_stats->DecodeError << " Decoded MBs=" <<
                dec_stats->DecodedMBs << " Concealed MBs=" <<
                dec_stats->ConcealedMBs << endl;
        }
    }
    else
    {
        cout << "No valid metadata for frame" << endl;
    }
}

#ifndef USE_NVBUF_TRANSFORM_API
/**
  * Send EndOfStream for converter.
  *
  * @param ctx : Decoder context
  */
static int
sendEOStoConverter(context_t *ctx)
{
    /* Check if converter is running */
    if (ctx->conv->output_plane.getStreamStatus())
    {
        NvBuffer *conv_buffer;
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(&planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;
        pthread_mutex_lock(&ctx->queue_lock);
        /* Wait till converter output buffer queue is empty. */
        while (ctx->conv_output_plane_buf_queue->empty())
        {
            pthread_cond_wait(&ctx->queue_cond, &ctx->queue_lock);
        }
        conv_buffer = ctx->conv_output_plane_buf_queue->front();
        ctx->conv_output_plane_buf_queue->pop();
        pthread_mutex_unlock(&ctx->queue_lock);

        v4l2_buf.index = conv_buffer->index;

        /*  Enqueue EOS buffer on converter output plane */
        return ctx->conv->output_plane.qBuffer(v4l2_buf, NULL);
    }
    return 0;
}
#endif

/**
  * Query and Set Capture plane.
  *
  * @param ctx : Decoder context
  */
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
    NvBufferCreateParams input_params = {0};
    NvBufferCreateParams cParams = {0};

    /* Get capture plane format from the decoder.
       This may change after resolution change event.
       Refer ioctl VIDIOC_G_FMT */
    ret = dec->capture_plane.getFormat(format);
    TEST_ERROR(ret < 0,
               "Error: Could not get format from decoder capture plane", error);

    /* Get the display resolution from the decoder.
       Refer ioctl VIDIOC_G_CROP */
    ret = dec->capture_plane.getCrop(crop);
    TEST_ERROR(ret < 0,
               "Error: Could not get crop from decoder capture plane", error);

    cout << "Video Resolution: " << crop.c.width << "x" << crop.c.height
        << endl;
    ctx->display_height = crop.c.height;
    ctx->display_width = crop.c.width;
#ifdef USE_NVBUF_TRANSFORM_API
    if(ctx->dst_dma_fd != -1)
    {
        NvBufferDestroy(ctx->dst_dma_fd);
        ctx->dst_dma_fd = -1;
    }

    /* Create PitchLinear output buffer for transform. */
    input_params.payloadType = NvBufferPayload_SurfArray;
    input_params.width = crop.c.width;
    input_params.height = crop.c.height;
    input_params.layout = NvBufferLayout_Pitch;
    input_params.colorFormat = ctx->out_pixfmt == 1 ? NvBufferColorFormat_NV12 :
                                            NvBufferColorFormat_YUV420;
    input_params.nvbuf_tag = NvBufferTag_VIDEO_DEC;

    ret = NvBufferCreateEx (&ctx->dst_dma_fd, &input_params);
    TEST_ERROR(ret == -1, "create dmabuf failed", error);
#else
    /* For file write, first deinitialize output and capture planes
       of video converter and then use the new resolution from
       decoder event resolution change. */
    if (ctx->conv)
    {
        ret = sendEOStoConverter(ctx);
        TEST_ERROR(ret < 0,
                   "Error while queueing EOS buffer on converter output",
                   error);

        ctx->conv->capture_plane.waitForDQThread(2000);

        ctx->conv->output_plane.deinitPlane();
        ctx->conv->capture_plane.deinitPlane();

        while(!ctx->conv_output_plane_buf_queue->empty())
        {
            ctx->conv_output_plane_buf_queue->pop();
        }
    }
#endif

    if (!ctx->disable_rendering)
    {
        /* Destroy the old instance of renderer as resolution might have changed */
        delete ctx->renderer;

        if (ctx->fullscreen)
        {
            /* Required for fullscreen */
            window_width = window_height = 0;
        }
        else if (ctx->window_width && ctx->window_height)
        {
            /* As specified by user on commandline */
            window_width = ctx->window_width;
            window_height = ctx->window_height;
        }
        else
        {
            /* Resolution got from the decoder */
            window_width = crop.c.width;
            window_height = crop.c.height;
        }

        /* If height or width are set to zero, EglRenderer creates a fullscreen
           window for Rendering */
        ctx->renderer =
                NvEglRenderer::createEglRenderer("renderer0", window_width,
                                           window_height, ctx->window_x,
                                           ctx->window_y);
        TEST_ERROR(!ctx->renderer,
                   "Error in setting up renderer. "
                   "Check if X is running or run with --disable-rendering",
                   error);
        if (ctx->stats)
        {
            /* Enable profiling for renderer if stats are requested. */
            ctx->renderer->enableProfiling();
        }

        /* Set fps for rendering. */
        ctx->renderer->setFPS(ctx->fps);
    }

    /* deinitPlane unmaps the buffers and calls REQBUFS with count 0 */
    dec->capture_plane.deinitPlane();
    if(ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
    {
        for(int index = 0 ; index < ctx->numCapBuffers ; index++)
        {
            if(ctx->dmabuff_fd[index] != 0)
            {
                ret = NvBufferDestroy (ctx->dmabuff_fd[index]);
                TEST_ERROR(ret < 0, "Failed to Destroy NvBuffer", error);
            }
        }
    }

    /* Not necessary to call VIDIOC_S_FMT on decoder capture plane.
       But decoder setCapturePlaneFormat function updates the class variables */
    ret = dec->setCapturePlaneFormat(format.fmt.pix_mp.pixelformat,
                                     format.fmt.pix_mp.width,
                                     format.fmt.pix_mp.height);
    TEST_ERROR(ret < 0, "Error in setting decoder capture plane format", error);

    ctx->video_height = format.fmt.pix_mp.height;
    ctx->video_width = format.fmt.pix_mp.width;
    /* Get the minimum buffers which have to be requested on the capture plane */
    ret = dec->getMinimumCapturePlaneBuffers(min_dec_capture_buffers);
    TEST_ERROR(ret < 0,
               "Error while getting value of minimum capture plane buffers",
               error);

    /* Request (min + 5) buffers, export and map buffers */
    if(ctx->capture_plane_mem_type == V4L2_MEMORY_MMAP)
    {
        ret =
            dec->capture_plane.setupPlane(V4L2_MEMORY_MMAP,
                                           min_dec_capture_buffers + 5, false,
                                           false);
        TEST_ERROR(ret < 0, "Error in decoder capture plane setup", error);
    }
    else if(ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
    {
        /* Set colorformats for relevant colorspaces. */
        switch(format.fmt.pix_mp.colorspace)
        {
            case V4L2_COLORSPACE_SMPTE170M:
                if (format.fmt.pix_mp.quantization == V4L2_QUANTIZATION_DEFAULT)
                {
                    cout << "Decoder colorspace ITU-R BT.601 "
                            "with standard range luma (16-235) \n";
                            cParams.colorFormat = NvBufferColorFormat_NV12;
                }
                else
                {
                    cout << "Decoder colorspace ITU-R BT.601 "
                            "with extended range luma (0-255) \n";
                            cParams.colorFormat = NvBufferColorFormat_NV12_ER;
                }
                break;
            case V4L2_COLORSPACE_REC709:
                if (format.fmt.pix_mp.quantization == V4L2_QUANTIZATION_DEFAULT)
                {
                    cout << "Decoder colorspace ITU-R BT.709 "
                            "with standard range luma (16-235) \n";
                            cParams.colorFormat = NvBufferColorFormat_NV12_709;
                }
                else
                {
                    cout << "Decoder colorspace ITU-R BT.709 "
                            "with extended range luma (0-255) \n";
                            cParams.colorFormat = NvBufferColorFormat_NV12_709_ER;
                }
                break;
            case V4L2_COLORSPACE_BT2020:
                {
                    cout << "Decoder colorspace ITU-R BT.2020 \n";
                            cParams.colorFormat = NvBufferColorFormat_NV12_2020;
                }
                break;
            default:
                cout << "supported colorspace details not"
                        "available, use default \n";
                if (format.fmt.pix_mp.quantization == V4L2_QUANTIZATION_DEFAULT)
                {
                    cout << "Decoder colorspace ITU-R BT.601 "
                            "with standard range luma (16-235) \n";
                            cParams.colorFormat = NvBufferColorFormat_NV12;
                }
                else
                {
                    cout << "Decoder colorspace ITU-R BT.601 "
                            "with extended range luma (0-255) \n";
                            cParams.colorFormat = NvBufferColorFormat_NV12_ER;
                }
                break;
        }
        ctx->numCapBuffers = min_dec_capture_buffers + 5;
        /* Create decoder capture plane buffers. */
        for (int index = 0; index < ctx->numCapBuffers; index++)
        {
            cParams.width = crop.c.width;
            cParams.height = crop.c.height;
            cParams.layout = NvBufferLayout_BlockLinear;
            cParams.payloadType = NvBufferPayload_SurfArray;
            cParams.nvbuf_tag = NvBufferTag_VIDEO_DEC;
            ret = NvBufferCreateEx(&ctx->dmabuff_fd[index], &cParams);
            TEST_ERROR(ret < 0, "Failed to create buffers", error);
        }
        /* Request buffers on decoder capture plane.
           Refer ioctl VIDIOC_REQBUFS */
        ret = dec->capture_plane.reqbufs(V4L2_MEMORY_DMABUF,ctx->numCapBuffers);
            TEST_ERROR(ret, "Error in request buffers on capture plane", error);
    }

#ifndef USE_NVBUF_TRANSFORM_API
    if (ctx->conv)
    {
        /* Set Converter output plane format.
           Refer ioctl VIDIOC_S_FMT */
        ret = ctx->conv->setOutputPlaneFormat(format.fmt.pix_mp.pixelformat,
                                              format.fmt.pix_mp.width,
                                              format.fmt.pix_mp.height,
                                              V4L2_NV_BUFFER_LAYOUT_BLOCKLINEAR);
        TEST_ERROR(ret < 0, "Error in converter output plane set format",
                   error);
        /* Set Converter capture plane format.
           Refer ioctl VIDIOC_S_FMT */
        ret = ctx->conv->setCapturePlaneFormat((ctx->out_pixfmt == 1 ?
                                                    V4L2_PIX_FMT_NV12M :
                                                    V4L2_PIX_FMT_YUV420M),
                                                crop.c.width,
                                                crop.c.height,
                                                V4L2_NV_BUFFER_LAYOUT_PITCH);
        TEST_ERROR(ret < 0, "Error in converter capture plane set format",
                   error);
        /* Set Converter crop rectangle. */
        ret = ctx->conv->setCropRect(0, 0, crop.c.width, crop.c.height);
        TEST_ERROR(ret < 0, "Error while setting crop rect", error);

        if (ctx->rescale_method) {
            /* Rescale full range [0-255] to limited range [16-235].
               Refer V4L2_CID_VIDEO_CONVERT_YUV_RESCALE_METHOD */
            ret = ctx->conv->setYUVRescale(ctx->rescale_method);
            TEST_ERROR(ret < 0, "Error while setting YUV rescale", error);
        }

        /* Request buffers on converter output plane.
           Refer ioctl VIDIOC_REQBUFS */
        ret =
            ctx->conv->output_plane.setupPlane(V4L2_MEMORY_DMABUF,
                                                dec->capture_plane.
                                                getNumBuffers(), false, false);
        TEST_ERROR(ret < 0, "Error in converter output plane setup", error);

        /* Request, Query and export converter capture plane buffers.
           Refer ioctl VIDIOC_REQBUFS, VIDIOC_QUERYBUF and VIDIOC_EXPBUF */
        ret =
            ctx->conv->capture_plane.setupPlane(V4L2_MEMORY_MMAP,
                                                 dec->capture_plane.
                                                 getNumBuffers(), true, false);
        TEST_ERROR(ret < 0, "Error in converter capture plane setup", error);

        /* Converter output plane STREAMON.
           Refer ioctl VIDIOC_STREAMON */
        ret = ctx->conv->output_plane.setStreamStatus(true);
        TEST_ERROR(ret < 0, "Error in converter output plane streamon", error);

        /* Converter capture plane STREAMON.
           Refer ioctl VIDIOC_STREAMON */
        ret = ctx->conv->capture_plane.setStreamStatus(true);
        TEST_ERROR(ret < 0, "Error in converter output plane streamoff", error);

        /* Add all empty conv output plane buffers to conv_output_plane_buf_queue */
        for (uint32_t i = 0; i < ctx->conv->output_plane.getNumBuffers(); i++)
        {
            ctx->conv_output_plane_buf_queue->push(ctx->conv->output_plane.
                    getNthBuffer(i));
        }

        /* Enqueue converter capture plane buffers. */
        for (uint32_t i = 0; i < ctx->conv->capture_plane.getNumBuffers(); i++)
        {
            struct v4l2_buffer v4l2_buf;
            struct v4l2_plane planes[MAX_PLANES];

            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            memset(planes, 0, sizeof(planes));

            v4l2_buf.index = i;
            v4l2_buf.m.planes = planes;
            ret = ctx->conv->capture_plane.qBuffer(v4l2_buf, NULL);
            TEST_ERROR(ret < 0, "Error Qing buffer at converter capture plane",
                       error);
        }
        /* Start deque thread for converter output plane. */
        ctx->conv->output_plane.startDQThread(ctx);
        /* Start deque thread for converter capture plane. */
        ctx->conv->capture_plane.startDQThread(ctx);

    }
#endif

    /* Decoder capture plane STREAMON.
       Refer ioctl VIDIOC_STREAMON */
    ret = dec->capture_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in decoder capture plane streamon", error);

    /* Enqueue all the empty decoder capture plane buffers. */
    for (uint32_t i = 0; i < dec->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf.memory = ctx->capture_plane_mem_type;
        if(ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
            v4l2_buf.m.planes[0].m.fd = ctx->dmabuff_fd[i];
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

/**
  * Decoder polling thread loop function.
  *
  * @param args : void arguments
  */
static void *decoder_pollthread_fcn(void *arg)
{

    context_t *ctx = (context_t *) arg;
    v4l2_ctrl_video_device_poll devicepoll;

    cout << "Starting Device Poll Thread " << endl;

    memset(&devicepoll, 0, sizeof(v4l2_ctrl_video_device_poll));

    /* Wait here until you are signalled to issue the Poll call.
       Check if the abort status is set , if so exit.
       Else issue the Poll on the decoder and block.
       When the Poll returns, signal the decoder thread to continue. */

    while (!ctx->got_error && !ctx->dec->isInError())
    {
        /* wait on polling semaphore */
        sem_wait(&ctx->pollthread_sema);

        if (ctx->got_eos)
        {
            cout << "Decoder got eos, exiting poll thread \n";
            return NULL;
        }

        devicepoll.req_events = POLLIN | POLLOUT | POLLERR | POLLPRI;

        /* This call shall wait in the v4l2 decoder library.
           Refer V4L2_CID_MPEG_VIDEO_DEVICE_POLL */
        ctx->dec->DevicePoll(&devicepoll);

        /* We can check the devicepoll.resp_events bitmask to see
           which events are set. */
        sem_post(&ctx->decoderthread_sema);
    }
    return NULL;
}

/**
  * Decoder capture thread loop function.
  *
  * @param args : void arguments
  */
static void *
dec_capture_loop_fcn(void *arg)
{
    context_t *ctx = (context_t *) arg;
    NvVideoDecoder *dec = ctx->dec;
    struct v4l2_event ev;
    int ret;

    cout << "Starting decoder capture loop thread" << endl;
    /* Need to wait for the first Resolution change event, so that
       the decoder knows the stream resolution and can allocate appropriate
       buffers when we call REQBUFS. */
    do
    {
        /* Refer ioctl VIDIOC_DQEVENT */
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
    while ((ev.type != V4L2_EVENT_RESOLUTION_CHANGE) && !ctx->got_error);

    /* Received the resolution change event, now can do query_and_set_capture. */
    if (!ctx->got_error)
        query_and_set_capture(ctx);

    /* Exit on error or EOS which is signalled in main() */
    while (!(ctx->got_error || dec->isInError()))
    {
        NvBuffer *dec_buffer;

        /* Check for Resolution change again.
           Refer ioctl VIDIOC_DQEVENT */
        ret = dec->dqEvent(ev, false);
        if (ret == 0)
        {
            switch (ev.type)
            {
                case V4L2_EVENT_RESOLUTION_CHANGE:
                    query_and_set_capture(ctx);
                    continue;
            }
        }

        /* Decoder capture loop */
        while (1)
        {
            struct v4l2_buffer v4l2_buf;
            struct v4l2_plane planes[MAX_PLANES];

            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            memset(planes, 0, sizeof(planes));
            v4l2_buf.m.planes = planes;

            /* Dequeue a filled buffer. */
            if (dec->capture_plane.dqBuffer(v4l2_buf, &dec_buffer, NULL, 0))
            {
                if (errno == EAGAIN)
                {
                    if (v4l2_buf.flags & V4L2_BUF_FLAG_LAST)
                    {
                        cout << "Got EoS at capture plane" << endl;
                        goto handle_eos;
                    }
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

            if (ctx->enable_metadata)
            {
                v4l2_ctrl_videodec_outputbuf_metadata dec_metadata;

                /* Get the decoder output metadata on capture-plane.
                   Refer V4L2_CID_MPEG_VIDEODEC_METADATA */
                ret = dec->getMetadata(v4l2_buf.index, dec_metadata);
                if (ret == 0)
                {
                    report_metadata(ctx, &dec_metadata);
                }
            }

            if (ctx->copy_timestamp && ctx->input_nalu && ctx->stats)
            {
              cout << "[" << v4l2_buf.index <<
                      "]" "dec capture plane dqB timestamp [" <<
                      v4l2_buf.timestamp.tv_sec << "s" <<
                      v4l2_buf.timestamp.tv_usec << "us]" << endl;
            }

            if (!ctx->disable_rendering && ctx->stats)
            {
                /* EglRenderer requires the fd of the 0th plane to render the buffer. */
                if(ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    dec_buffer->planes[0].fd = ctx->dmabuff_fd[v4l2_buf.index];
                ctx->renderer->render(dec_buffer->planes[0].fd);
            }

             /* If we need to write to file or display the buffer, give
               the buffer to video converter output plane instead of
               returning the buffer back to decoder capture plane. */
            if (ctx->out_file || (!ctx->disable_rendering && !ctx->stats))
            {
#ifndef USE_NVBUF_TRANSFORM_API
                NvBuffer *conv_buffer;
                struct v4l2_buffer conv_output_buffer;
                struct v4l2_plane conv_planes[MAX_PLANES];

                memset(&conv_output_buffer, 0, sizeof(conv_output_buffer));
                memset(conv_planes, 0, sizeof(conv_planes));
                conv_output_buffer.m.planes = conv_planes;

                /* Get an empty conv output plane buffer from conv_output_plane_buf_queue */
                pthread_mutex_lock(&ctx->queue_lock);
                while (ctx->conv_output_plane_buf_queue->empty())
                {
                    pthread_cond_wait(&ctx->queue_cond, &ctx->queue_lock);
                }
                conv_buffer = ctx->conv_output_plane_buf_queue->front();
                ctx->conv_output_plane_buf_queue->pop();
                pthread_mutex_unlock(&ctx->queue_lock);

                conv_output_buffer.index = conv_buffer->index;
                if (ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    dec_buffer->planes[0].fd = ctx->dmabuff_fd[v4l2_buf.index];

                /* enqueue converter output plane buffer. */
                if (ctx->conv->output_plane.
                    qBuffer(conv_output_buffer, dec_buffer) < 0)
                {
                    abort(ctx);
                    cerr <<
                        "Error while queueing buffer at converter output plane"
                        << endl;
                    break;
                }
#else
                /* Clip & Stitch can be done by adjusting rectangle */
                NvBufferRect src_rect, dest_rect;
                src_rect.top = 0;
                src_rect.left = 0;
                src_rect.width = ctx->display_width;
                src_rect.height = ctx->display_height;
                dest_rect.top = 0;
                dest_rect.left = 0;
                dest_rect.width = ctx->display_width;
                dest_rect.height = ctx->display_height;

                NvBufferTransformParams transform_params;
                memset(&transform_params,0,sizeof(transform_params));
                /* Indicates which of the transform parameters are valid */
                transform_params.transform_flag = NVBUFFER_TRANSFORM_FILTER;
                transform_params.transform_flip = NvBufferTransform_None;
                transform_params.transform_filter = NvBufferTransform_Filter_Smart;
                transform_params.src_rect = src_rect;
                transform_params.dst_rect = dest_rect;

                if(ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    dec_buffer->planes[0].fd = ctx->dmabuff_fd[v4l2_buf.index];
                /* Perform Blocklinear to PitchLinear conversion. */
                ret = NvBufferTransform(dec_buffer->planes[0].fd,
                                        ctx->dst_dma_fd, &transform_params);
                if (ret == -1)
                {
                    cerr << "Transform failed" << endl;
                    break;
                }

                /* Write raw video frame to file */
                if (!ctx->stats && ctx->out_file)
                {
                    /* Dumping two planes of NV12 and three for I420 */
                    dump_dmabuf(ctx->dst_dma_fd, 0, ctx->out_file);
                    dump_dmabuf(ctx->dst_dma_fd, 1, ctx->out_file);
                    if (ctx->out_pixfmt != 1)
                    {
                        dump_dmabuf(ctx->dst_dma_fd, 2, ctx->out_file);
                    }
                }

                if (!ctx->stats && !ctx->disable_rendering)
                {
                    ctx->renderer->render(ctx->dst_dma_fd);
                }

                /* If not writing to file, Queue the buffer back once it has been used. */
                if(ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    v4l2_buf.m.planes[0].m.fd = ctx->dmabuff_fd[v4l2_buf.index];
                if (dec->capture_plane.qBuffer(v4l2_buf, NULL) < 0)
                {
                    abort(ctx);
                    cerr <<
                        "Error while queueing buffer at decoder capture plane"
                        << endl;
                    break;
                }
#endif
            }
            else
            {
                /* If not writing to file, Queue the buffer back once it has been used. */
                if(ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    v4l2_buf.m.planes[0].m.fd = ctx->dmabuff_fd[v4l2_buf.index];
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
    }
handle_eos:

#ifndef USE_NVBUF_TRANSFORM_API
    /* Send EOS to converter */
    if (ctx->conv)
    {
        if (sendEOStoConverter(ctx) < 0)
        {
            cerr << "Error while queueing EOS buffer on converter output"
                 << endl;
        }
    }
#endif
    cout << "Exiting decoder capture loop thread" << endl;
    return NULL;
}

/**
  * Set the default values for decoder context members.
  *
  * @param ctx : Pointer to multiple decoder contexts
  * @param stream_stats : Pointer to the decoding stats
  */
static void
set_defaults(context_t ** ctx,fps_stats **stream_stats)
{
    for ( int i = 0 ; i< num_files ; i++ )
    {
        ctx[i] = (context_t *) malloc(sizeof(context_t));
        stream_stats[i] = (fps_stats *)malloc(sizeof(fps_stats));
        memset(ctx[i], 0, sizeof(context_t));
        memset(stream_stats[i], 0 , sizeof(stream_stats));
        ctx[i]->thread_num = i;
        ctx[i]->fullscreen = false;
        ctx[i]->window_height = 0;
        ctx[i]->window_width = 0;
        ctx[i]->window_x = 0;
        ctx[i]->window_y = 0;
        ctx[i]->out_pixfmt = 1;
        ctx[i]->fps = 30;
        ctx[i]->output_plane_mem_type = V4L2_MEMORY_MMAP;
        ctx[i]->capture_plane_mem_type = V4L2_MEMORY_DMABUF;
        ctx[i]->vp9_file_header_flag = 0;
        ctx[i]->vp8_file_header_flag = 0;
        ctx[i]->stress_test = 1;
        ctx[i]->copy_timestamp = false;
        ctx[i]->flag_copyts = false;
        ctx[i]->disable_rendering = true;
        ctx[i]->start_ts = 0;
        ctx[i]->file_count = 1;
        ctx[i]->dec_fps = 30;
        ctx[i]->dst_dma_fd = -1;
        ctx[i]->loop_count = 0;
        ctx[i]->blocking_mode = 1;
#ifndef USE_NVBUF_TRANSFORM_API
        ctx[i]->conv_output_plane_buf_queue = new queue < NvBuffer * >;
        ctx[i]->rescale_method = V4L2_YUV_RESCALE_NONE;
#endif
        pthread_mutex_init(&ctx[i]->queue_lock, NULL);
        pthread_cond_init(&ctx[i]->queue_cond, NULL);
    }
}

/**
  * Decode processing function for non-blocking mode.
  *
  * @param ctx               : Decoder context
  * @param eos               : end of stream
  * @param current_file      : current file
  * @param current_loop      : iterator count
  * @param nalu_parse_buffer : input parsed nal unit
  */
static bool
decoder_proc_nonblocking(context_t &ctx, bool eos, uint32_t current_file,
                    char *nalu_parse_buffer)
{
     /*  NOTE: In non-blocking mode, we will have this function do below things:
              1) Issue signal to PollThread so it starts Poll and wait until we are signalled.
              2) After we are signalled, it means there is something to dequeue, either output plane
                 or capture plane or there's an event.
              3) Try dequeuing from all three and then act appropriately.
              4) After enqueuing go back to the same loop. */

    /* Since all the output plane buffers have been queued, we first need to
       dequeue a buffer from output plane before we can read new data into it
       and queue it again. */
    int allow_DQ = true;
    int ret = 0;
    struct v4l2_buffer temp_buf;
    struct v4l2_event ev;

    while (!ctx.got_error && !ctx.dec->isInError())
    {
        struct v4l2_buffer v4l2_output_buf;
        struct v4l2_plane output_planes[MAX_PLANES];

        struct v4l2_buffer v4l2_capture_buf;
        struct v4l2_plane capture_planes[MAX_PLANES];

        NvBuffer *output_buffer = NULL;
        NvBuffer *capture_buffer = NULL;

        memset(&v4l2_output_buf, 0, sizeof(v4l2_output_buf));
        memset(output_planes, 0, sizeof(output_planes));
        v4l2_output_buf.m.planes = output_planes;

        memset(&v4l2_capture_buf, 0, sizeof(v4l2_capture_buf));
        memset(capture_planes, 0, sizeof(capture_planes));
        v4l2_capture_buf.m.planes = capture_planes;

        /* Call for SetPollInterrupt.
           Refer V4L2_CID_MPEG_SET_POLL_INTERRUPT */
        ctx.dec->SetPollInterrupt();

        /* Since buffers have been queued, issue a post to start polling and
           then wait here. */
        sem_post(&ctx.pollthread_sema);
        sem_wait(&ctx.decoderthread_sema);

        /* Call for dequeuing an event.
           Refer ioctl VIDIOC_DQEVENT */
        ret = ctx.dec->dqEvent(ev, 0);
        if (ret == 0)
        {
            if (ev.type == V4L2_EVENT_RESOLUTION_CHANGE)
            {
                /* Received the resolution change event, now can do query_and_set_capture. */
                cout << "Got V4L2_EVENT_RESOLUTION_CHANGE EVENT \n";
                query_and_set_capture(&ctx);
            }
        }

        /* dequeue from the output plane and enqueue back the buffers after reading. */
        while (1)
        {
            if ( (eos) && (ctx.dec->output_plane.getNumQueuedBuffers() == 0) )
            {
                cout << "Done processing all the buffers returning \n";
                return true;
            }

            /* dequeue a buffer for output plane. */
            if (allow_DQ)
            {
                ret = ctx.dec->output_plane.dqBuffer(v4l2_output_buf, &output_buffer, NULL, 0);
                if (ret < 0)
                {
                    if (errno == EAGAIN)
                        goto check_capture_buffers;
                    else
                    {
                        cerr << "Error DQing buffer at output plane" << endl;
                        abort(&ctx);
                        break;
                    }
                }
            }
            else
            {
                allow_DQ = true;
                memcpy(&v4l2_output_buf,&temp_buf,sizeof(v4l2_buffer));
                output_buffer = ctx.dec->output_plane.getNthBuffer(v4l2_output_buf.index);
            }

            if ((v4l2_output_buf.flags & V4L2_BUF_FLAG_ERROR) && ctx.enable_input_metadata)
            {
                v4l2_ctrl_videodec_inputbuf_metadata dec_input_metadata;

                /* Get the decoder input metadata.
                   Refer V4L2_CID_MPEG_VIDEODEC_INPUT_METADATA */
                ret = ctx.dec->getInputMetadata(v4l2_output_buf.index, dec_input_metadata);
                if (ret == 0)
                {
                    ret = report_input_metadata(&ctx, &dec_input_metadata);
                    if (ret == -1)
                    {
                        cerr << "Error with input stream header parsing" << endl;
                    }
                }
            }

            if (eos)
            {
                /* Got End Of Stream, no more queueing of buffers on OUTPUT plane. */
                goto check_capture_buffers;
            }

            if ((ctx.decoder_pixfmt == V4L2_PIX_FMT_H264) ||
                    (ctx.decoder_pixfmt == V4L2_PIX_FMT_H265) ||
                    (ctx.decoder_pixfmt == V4L2_PIX_FMT_MPEG2) ||
                    (ctx.decoder_pixfmt == V4L2_PIX_FMT_MPEG4))
            {
                if (ctx.input_nalu)
                {
                    /* read the input nal unit. */
                    read_decoder_input_nalu(ctx.in_file, output_buffer, nalu_parse_buffer,
                            CHUNK_SIZE, &ctx);
                }
                else
                {
                    /* read the input chunks. */
                    read_decoder_input_chunk(ctx.in_file, output_buffer);
                }
            }
            if (ctx.decoder_pixfmt == V4L2_PIX_FMT_VP9 || ctx.decoder_pixfmt == V4L2_PIX_FMT_VP8)
            {
                ret = read_vpx_decoder_input_chunk(&ctx, output_buffer);
                if (ret != 0)
                    cerr << "Couldn't read VP9 chunk" << endl;
            }
            v4l2_output_buf.m.planes[0].bytesused = output_buffer->planes[0].bytesused;

            if (ctx.input_nalu && ctx.copy_timestamp && ctx.flag_copyts)
            {
                /* Update the timestamp. */
                v4l2_output_buf.flags |= V4L2_BUF_FLAG_TIMESTAMP_COPY;
                ctx.timestamp += ctx.timestampincr;
                v4l2_output_buf.timestamp.tv_sec = ctx.timestamp / (MICROSECOND_UNIT);
                v4l2_output_buf.timestamp.tv_usec = ctx.timestamp % (MICROSECOND_UNIT);
            }

            /* enqueue a buffer for output plane. */
            ret = ctx.dec->output_plane.qBuffer(v4l2_output_buf, NULL);
            if (ret < 0)
            {
                cerr << "Error Qing buffer at output plane" << endl;
                abort(&ctx);
                break;
            }
            if (v4l2_output_buf.m.planes[0].bytesused == 0)
            {
                eos = true;
                cout << "Input file read complete" << endl;
                goto check_capture_buffers;
            }
        }

check_capture_buffers:

        /* Dequeue from the capture plane and write them to file and enqueue back */
        while (1)
        {
            if (!ctx.dec->capture_plane.getStreamStatus())
            {
                cout << "Capture plane not ON, skipping capture plane \n";
                break;
            }

            /* Dequeue a filled buffer */
            ret = ctx.dec->capture_plane.dqBuffer(v4l2_capture_buf, &capture_buffer, NULL, 0);
            if (ret < 0)
            {
                if (errno == EAGAIN)
                    break;
                else
                {
                    abort(&ctx);
                    cerr << "Error while calling dequeue at capture plane" <<
                        endl;
                }
                break;
            }
            if (capture_buffer == NULL)
            {
                cout << "Got CAPTURE BUFFER NULL \n";
                break;
            }

            if (ctx.enable_metadata)
            {
                v4l2_ctrl_videodec_outputbuf_metadata dec_metadata;

                /* Get the decoder output metadata on capture-plane.
                   Refer V4L2_CID_MPEG_VIDEODEC_METADATA */
                ret = ctx.dec->getMetadata(v4l2_capture_buf.index, dec_metadata);
                if (ret == 0)
                {
                    report_metadata(&ctx, &dec_metadata);
                }
            }

            if (ctx.copy_timestamp && ctx.input_nalu && ctx.stats)
            {
              cout << "[" << v4l2_capture_buf.index <<
                      "]" "dec capture plane dqB timestamp [" <<
                      v4l2_capture_buf.timestamp.tv_sec <<
                      "s" << v4l2_capture_buf.timestamp.tv_usec <<
                      "us]" << endl;
            }

            if (!ctx.disable_rendering && ctx.stats)
            {
                /* Rendering the buffer.
                   NOTE: EglRenderer requires the fd of the 0th plane to render the buffer. */
                if(ctx.capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    capture_buffer->planes[0].fd = ctx.dmabuff_fd[v4l2_capture_buf.index];
                if (ctx.renderer->render(capture_buffer->planes[0].fd) == -1)
                {
                    abort(&ctx);
                    cerr << "Error while queueing buffer for rendering "
                            << endl;
                    break;
                }
            }

            /* Get the decoded buffer data dumped to file. */
            if (ctx.out_file || (!ctx.disable_rendering && !ctx.stats))
            {
                NvBufferRect src_rect, dest_rect;
                src_rect.top = 0;
                src_rect.left = 0;
                src_rect.width = ctx.display_width;
                src_rect.height = ctx.display_height;
                dest_rect.top = 0;
                dest_rect.left = 0;
                dest_rect.width = ctx.display_width;
                dest_rect.height = ctx.display_height;

                NvBufferTransformParams transform_params;
                /* Indicates which of the transform parameters are valid */
                memset(&transform_params, 0, sizeof(transform_params));
                transform_params.transform_flag = NVBUFFER_TRANSFORM_FILTER;
                transform_params.transform_flip = NvBufferTransform_None;
                transform_params.transform_filter = NvBufferTransform_Filter_Smart;
                transform_params.src_rect = src_rect;
                transform_params.dst_rect = dest_rect;

                if(ctx.capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    capture_buffer->planes[0].fd = ctx.dmabuff_fd[v4l2_capture_buf.index];
                /* Perform Blocklinear to PitchLinear conversion. */
                ret = NvBufferTransform(capture_buffer->planes[0].fd,
                                        ctx.dst_dma_fd, &transform_params);
                if (ret == -1)
                {
                    cerr << "Transform failed" << endl;
                    break;
                }

                /* Write raw video frame to file */
                if (!ctx.stats && ctx.out_file)
                {
                    /* Dumping two planes of NV12 and three for I420 */
                    cout << "Writing to file \n";
                    dump_dmabuf(ctx.dst_dma_fd, 0, ctx.out_file);
                    dump_dmabuf(ctx.dst_dma_fd, 1, ctx.out_file);
                    if (ctx.out_pixfmt != 1)
                    {
                        dump_dmabuf(ctx.dst_dma_fd, 2, ctx.out_file);
                    }
                }
                if (!ctx.stats && !ctx.disable_rendering)
                {
                    ctx.renderer->render(ctx.dst_dma_fd);
                }
                /* Queue the buffer back once it has been used.
                   NOTE: If we are not rendering, queue the buffer back here immediately. */
                if(ctx.capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    v4l2_capture_buf.m.planes[0].m.fd = ctx.dmabuff_fd[v4l2_capture_buf.index];
                if (ctx.dec->capture_plane.qBuffer(v4l2_capture_buf, NULL) < 0)
                {
                    abort(&ctx);
                    cerr << "Error while queueing buffer at decoder capture plane"
                            << endl;
                    break;
                }
            }
        }
    }
    return eos;
}

/**
  * Decode processing function for blocking mode.
  *
  * @param ctx               : Decoder context
  * @param eos               : end of stream
  * @param current_file      : current file
  * @param current_loop      : iterator count
  * @param nalu_parse_buffer : input parsed nal unit
  */
static bool
decoder_proc_blocking(context_t &ctx, bool eos, uint32_t current_file,
                        char *nalu_parse_buffer)
{

    int allow_DQ = true;
    int ret = 0;
    struct v4l2_buffer temp_buf;

    /* Since all the output plane buffers have been queued, we first need to
       dequeue a buffer from output plane before we can read new data into it
       and queue it again. */
    while (!eos && !ctx.got_error && !ctx.dec->isInError())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        /* dequeue a buffer for output plane. */
        v4l2_buf.m.planes = planes;

        if(allow_DQ)
        {
            ret = ctx.dec->output_plane.dqBuffer(v4l2_buf, &buffer, NULL, -1);
            if (ret < 0)
            {
                cerr << "Error DQing buffer at output plane" << endl;
                abort(&ctx);
                break;
            }
        }
        else
        {
            allow_DQ = true;
            memcpy(&v4l2_buf,&temp_buf,sizeof(v4l2_buffer));
            buffer = ctx.dec->output_plane.getNthBuffer(v4l2_buf.index);
        }

        if ((v4l2_buf.flags & V4L2_BUF_FLAG_ERROR) && ctx.enable_input_metadata)
        {
            v4l2_ctrl_videodec_inputbuf_metadata dec_input_metadata;

            /* Get the decoder input metadata.
               Refer V4L2_CID_MPEG_VIDEODEC_INPUT_METADATA */
            ret = ctx.dec->getInputMetadata(v4l2_buf.index, dec_input_metadata);
            if (ret == 0)
            {
                ret = report_input_metadata(&ctx, &dec_input_metadata);
                if (ret == -1)
                {
                  cerr << "Error with input stream header parsing" << endl;
                }
            }
        }

        if ((ctx.decoder_pixfmt == V4L2_PIX_FMT_H264) ||
                (ctx.decoder_pixfmt == V4L2_PIX_FMT_H265) ||
                (ctx.decoder_pixfmt == V4L2_PIX_FMT_MPEG2) ||
                (ctx.decoder_pixfmt == V4L2_PIX_FMT_MPEG4))
        {
            if (ctx.input_nalu)
            {
                /* read the input nal unit. */
                read_decoder_input_nalu(ctx.in_file, buffer, nalu_parse_buffer,
                        CHUNK_SIZE, &ctx);
            }
            else
            {
                /* read the input chunks. */
                read_decoder_input_chunk(ctx.in_file, buffer);
            }
        }
        if (ctx.decoder_pixfmt == V4L2_PIX_FMT_VP9 || ctx.decoder_pixfmt == V4L2_PIX_FMT_VP8)
        {
            /* read the input chunks. */
            ret = read_vpx_decoder_input_chunk(&ctx, buffer);
            if (ret != 0)
                cerr << "Couldn't read VP9 chunk" << endl;
        }
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;

        if (ctx.input_nalu && ctx.copy_timestamp && ctx.flag_copyts)
        {
          /* Update the timestamp. */
          v4l2_buf.flags |= V4L2_BUF_FLAG_TIMESTAMP_COPY;
          ctx.timestamp += ctx.timestampincr;
          v4l2_buf.timestamp.tv_sec = ctx.timestamp / (MICROSECOND_UNIT);
          v4l2_buf.timestamp.tv_usec = ctx.timestamp % (MICROSECOND_UNIT);
        }

        /* enqueue a buffer for output plane. */
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
    return eos;
}

/**
  * Decode processing function.
  *
  * @param ctx  : Decoder context
  */
static void *
decode_proc(void * p_ctx)
{
    context_t ctx = *(context_t *)p_ctx;
    int ret = 0;
    int error = 0;
    uint32_t current_file = 0;
    uint32_t i;
    bool eos = false;
    char *nalu_parse_buffer = NULL;
    int * perror = (int *)malloc(sizeof(int));
    NvApplicationProfiler &profiler = NvApplicationProfiler::getProfilerInstance();
    NvElementProfiler::NvElementProfilerData data;

    /* Create NvVideoDecoder object for blocking or non-blocking I/O mode. */
    if (ctx.blocking_mode)
    {
        cout << "Creating decoder in blocking mode \n";
        ctx.dec = NvVideoDecoder::createVideoDecoder("dec0");
    }
    else
    {
        cout << "Creating decoder in non-blocking mode \n";
        ctx.dec = NvVideoDecoder::createVideoDecoder("dec0", O_NONBLOCK);
    }
    TEST_ERROR(!ctx.dec, "Could not create decoder", cleanup);

    /* Enable profiling for decoder if stats are requested. */
    if (ctx.stats)
    {
        profiler.start(NvApplicationProfiler::DefaultSamplingInterval);
        ctx.dec->enableProfiling();
    }

    /* Subscribe to Resolution change event.
       Refer ioctl VIDIOC_SUBSCRIBE_EVENT */
    ret = ctx.dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE, 0, 0);
    TEST_ERROR(ret < 0, "Could not subscribe to V4L2_EVENT_RESOLUTION_CHANGE",
               cleanup);

    /* Set format on the output plane.
       Refer ioctl VIDIOC_S_FMT */
    ret = ctx.dec->setOutputPlaneFormat(ctx.decoder_pixfmt, CHUNK_SIZE);
    TEST_ERROR(ret < 0, "Could not set output plane format", cleanup);

    /* Configure for frame input mode for decoder.
       Refer V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT */
    if (ctx.input_nalu)
    {
        /* Input to the decoder will be nal units. */
        nalu_parse_buffer = new char[CHUNK_SIZE];
        printf("Setting frame input mode to 0 \n");
        ret = ctx.dec->setFrameInputMode(0);
        TEST_ERROR(ret < 0,
                "Error in decoder setFrameInputMode", cleanup);
    }
    else
    {
        /* Input to the decoder will be a chunk of bytes.
           NOTE: Set V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT control to
                 false so that application can send chunks of encoded data instead
                 of forming complete frames. */
        printf("Setting frame input mode to 1 \n");
        ret = ctx.dec->setFrameInputMode(1);
        TEST_ERROR(ret < 0,
                "Error in decoder setFrameInputMode", cleanup);
    }

    /* Disable decoder DPB management.
       NOTE: V4L2_CID_MPEG_VIDEO_DISABLE_DPB should be set after output plane
             set format */
    if (ctx.disable_dpb)
    {
        ret = ctx.dec->disableDPB();
        TEST_ERROR(ret < 0, "Error in decoder disableDPB", cleanup);
    }

    /* Enable decoder error and metadata reporting.
       Refer V4L2_CID_MPEG_VIDEO_ERROR_REPORTING */
    if (ctx.enable_metadata || ctx.enable_input_metadata)
    {
        ret = ctx.dec->enableMetadataReporting();
        TEST_ERROR(ret < 0, "Error while enabling metadata reporting", cleanup);
    }

    /* Set the skip frames property of the decoder.
       Refer V4L2_CID_MPEG_VIDEO_SKIP_FRAMES */
    if (ctx.skip_frames)
    {
        ret = ctx.dec->setSkipFrames(ctx.skip_frames);
        TEST_ERROR(ret < 0, "Error while setting skip frames param", cleanup);
    }

    /* Query, Export and Map the output plane buffers so can read
       encoded data into the buffers. */
    if (ctx.output_plane_mem_type == V4L2_MEMORY_MMAP)
    {
        /* configure decoder output plane for MMAP io-mode.
           Refer ioctl VIDIOC_REQBUFS, VIDIOC_QUERYBUF and VIDIOC_EXPBUF */
        ret = ctx.dec->output_plane.setupPlane(V4L2_MEMORY_MMAP, 10, true, false);
    }
    else if (ctx.output_plane_mem_type == V4L2_MEMORY_USERPTR)
    {
        /* configure decoder output plane for USERPTR io-mode.
           Refer ioctl VIDIOC_REQBUFS */
        ret = ctx.dec->output_plane.setupPlane(V4L2_MEMORY_USERPTR, 10, false, true);
    }

    TEST_ERROR(ret < 0, "Error while setting up output plane", cleanup);

    ctx.in_file = new ifstream(ctx.in_file_path);
    TEST_ERROR(!ctx.in_file->is_open(), "Error opening input file", cleanup);

    if (ctx.out_file_path)
    {
        ctx.out_file = new ofstream(ctx.out_file_path);
        TEST_ERROR(!ctx.out_file->is_open(), "Error opening output file",
                   cleanup);
    }

#ifndef USE_NVBUF_TRANSFORM_API
    if (ctx.out_file || (!ctx.disable_rendering && !ctx.stats))
    {
        /* Create converter object for Block-linear to Pitch-linear
           transform required for writing decoded raw video to file. */
        ctx.conv = NvVideoConverter::createVideoConverter("conv0");
        TEST_ERROR(!ctx.conv, "Could not create video converter", cleanup);
        /* Set dqbuffer thread callback for converter output-plane. */
        ctx.conv->output_plane.
            setDQThreadCallback(conv0_output_dqbuf_thread_callback);
        /* Set dqbuffer thread callback for converter capture-plane. */
        ctx.conv->capture_plane.
            setDQThreadCallback(conv0_capture_dqbuf_thread_callback);

        if (ctx.stats)
        {
            /* Enable profiling for converter if stats are requested. */
            ctx.conv->enableProfiling();
        }
    }
#endif

    /* Start stream processing on decoder output-plane.
       Refer ioctl VIDIOC_STREAMON */
    ret = ctx.dec->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in output plane stream on", cleanup);

    /* Create threads for decoder output */
    if (ctx.blocking_mode)
    {
        pthread_create(&ctx.dec_capture_loop, NULL, dec_capture_loop_fcn, &ctx);
        char dec_capture_plane[16] = "DecCapplane";
        string s = to_string(ctx.thread_num);
        strcat(dec_capture_plane, s.c_str());
        /* Set thread name for decoder Capture Plane threads. */
        pthread_setname_np(ctx.dec_capture_loop, dec_capture_plane);

    }
    else
    {
        sem_init(&ctx.pollthread_sema, 0, 0);
        sem_init(&ctx.decoderthread_sema, 0, 0);
        pthread_create(&ctx.dec_pollthread, NULL, decoder_pollthread_fcn, &ctx);
        cout << "Created the PollThread and Decoder Thread \n";
        char dec_poll[16] = "PollThread";
        string s = to_string(ctx.thread_num);
        strcat(dec_poll, s.c_str());
        /* Set thread name for decoder poll threads. */
        pthread_setname_np(ctx.dec_pollthread, dec_poll);
    }

    if (ctx.copy_timestamp && ctx.input_nalu) {
      ctx.timestamp = (ctx.start_ts * MICROSECOND_UNIT);
      ctx.timestampincr = (MICROSECOND_UNIT * 16) / ((uint32_t) (ctx.dec_fps * 16));
    }

    /* Read encoded data and enqueue all the output plane buffers.
       Exit loop in case file read is complete. */
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
        if ((ctx.decoder_pixfmt == V4L2_PIX_FMT_H264) ||
                (ctx.decoder_pixfmt == V4L2_PIX_FMT_H265) ||
                (ctx.decoder_pixfmt == V4L2_PIX_FMT_MPEG2) ||
                (ctx.decoder_pixfmt == V4L2_PIX_FMT_MPEG4))
        {
            if (ctx.input_nalu)
            {
                /* read the input nal unit. */
                read_decoder_input_nalu(ctx.in_file, buffer, nalu_parse_buffer,
                        CHUNK_SIZE, &ctx);
            }
            else
            {
                /* read the input chunks. */
                read_decoder_input_chunk(ctx.in_file, buffer);
            }
        }
        if (ctx.decoder_pixfmt == V4L2_PIX_FMT_VP9 || ctx.decoder_pixfmt == V4L2_PIX_FMT_VP8)
        {
            /* read the input chunks. */
            ret = read_vpx_decoder_input_chunk(&ctx, buffer);
            if (ret != 0)
                cerr << "Couldn't read VP9 chunk" << endl;
        }

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;

        if (ctx.input_nalu && ctx.copy_timestamp && ctx.flag_copyts)
        {
          /* Update the timestamp. */
          v4l2_buf.flags |= V4L2_BUF_FLAG_TIMESTAMP_COPY;
          ctx.timestamp += ctx.timestampincr;
          v4l2_buf.timestamp.tv_sec = ctx.timestamp / (MICROSECOND_UNIT);
          v4l2_buf.timestamp.tv_usec = ctx.timestamp % (MICROSECOND_UNIT);
        }

        /* It is necessary to queue an empty buffer to signal EOS to the decoder
           i.e. set v4l2_buf.m.planes[0].bytesused = 0 and queue the buffer. */
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
    if (ctx.blocking_mode)
        eos = decoder_proc_blocking(ctx, eos, current_file, nalu_parse_buffer);
    else
        eos = decoder_proc_nonblocking(ctx, eos, current_file, nalu_parse_buffer);

    /* After sending EOS, all the buffers from output plane should be dequeued.
       and after that capture plane loop should be signalled to stop. */
    if (ctx.blocking_mode)
    {
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
            if (v4l2_buf.m.planes[0].bytesused == 0)
            {
                cout << "Got EoS at output plane"<< endl;
                break;
            }

            if ((v4l2_buf.flags & V4L2_BUF_FLAG_ERROR) &&
                    ctx.enable_input_metadata)
            {
                v4l2_ctrl_videodec_inputbuf_metadata dec_input_metadata;

                /* Get the decoder input metadata.
                   Refer V4L2_CID_MPEG_VIDEODEC_INPUT_METADATA */
                ret = ctx.dec->getInputMetadata(v4l2_buf.index,
                                                dec_input_metadata);
                if (ret == 0)
                {
                    ret = report_input_metadata(&ctx, &dec_input_metadata);
                    if (ret == -1)
                    {
                      cerr << "Error with input stream header parsing" << endl;
                      abort(&ctx);
                      break;
                    }
                }
            }
        }
    }
    /* Signal EOS to the decoder capture loop */
    ctx.got_eos = true;
#ifndef USE_NVBUF_TRANSFORM_API
    if (ctx.conv)
    {
        ctx.conv->capture_plane.waitForDQThread(-1);
    }
#endif

cleanup:
    if (ctx.blocking_mode && ctx.dec_capture_loop)
    {
        pthread_join(ctx.dec_capture_loop, NULL);
    }
    else if (!ctx.blocking_mode)
    {
        /* Clear the poll interrupt to get the decoder's poll thread out. */
        ctx.dec->ClearPollInterrupt();
        /* If Pollthread is waiting on, signal it to exit the thread. */
        sem_post(&ctx.pollthread_sema);
        pthread_join(ctx.dec_pollthread, NULL);
    }

    if (ctx.stats)
    {
        profiler.stop();
        ctx.dec->getProfilingData(data);
        stream_stats[ctx.thread_num]->filename = strdup(ctx.in_file_path);
        stream_stats[ctx.thread_num]->data = data;
        stream_stats[ctx.thread_num]->thread_num = ctx.thread_num;

#ifndef USE_NVBUF_TRANSFORM_API
        if (ctx.conv)
        {
            ctx.conv->printProfilingStats(cout);
        }
#endif
        if (ctx.renderer)
        {
            ctx.renderer->printProfilingStats(cout);
        }
    }

    if(ctx.capture_plane_mem_type == V4L2_MEMORY_DMABUF)
    {
        for(int index = 0 ; index < ctx.numCapBuffers ; index++)
        {
            if(ctx.dmabuff_fd[index] != 0)
            {
                ret = NvBufferDestroy (ctx.dmabuff_fd[index]);
                if(ret < 0)
                {
                    cerr << "Failed to Destroy NvBuffer" << endl;
                }
            }
        }
    }
#ifndef USE_NVBUF_TRANSFORM_API
    if (ctx.conv && ctx.conv->isInError())
    {
        cerr << "Converter is in error" << endl;
        error = 1;
    }
#endif
    if (ctx.dec && ctx.dec->isInError())
    {
        cerr << "Decoder is in error" << endl;
        error = 1;
    }

    if (ctx.got_error)
    {
        error = 1;
    }

    /* The decoder destructor does all the cleanup i.e set streamoff on output and
       capture planes, unmap buffers, tell decoder to deallocate buffer (reqbufs
       ioctl with count = 0), and finally call v4l2_close on the fd. */
    delete ctx.dec;
#ifndef USE_NVBUF_TRANSFORM_API
    delete ctx.conv;
#endif
    /* Similarly, EglRenderer destructor does all the cleanup */
    delete ctx.renderer;
    delete ctx.in_file;
    delete ctx.out_file;
#ifndef USE_NVBUF_TRANSFORM_API
    delete ctx.conv_output_plane_buf_queue;
#else
    if(ctx.dst_dma_fd != -1)
    {
        NvBufferDestroy(ctx.dst_dma_fd);
        ctx.dst_dma_fd = -1;
    }
#endif
    delete[] nalu_parse_buffer;
    free (ctx.in_file_path);
    free (ctx.out_file_path);
    if (!ctx.blocking_mode)
    {
        sem_destroy(&ctx.pollthread_sema);
        sem_destroy(&ctx.decoderthread_sema);
    }

    if(-error == 0)
    {
        cout << "Instance " << ctx.thread_num << " executed sucessfully." << endl;
    }
    else
    {
        cout << "Instance " << ctx.thread_num << " Failed." << endl;
    }
    free (p_ctx);
    *perror = -error;
    return (perror);
}

/**
  * Start of video Decode application.
  *
  * @param argc : Argument Count
  * @param argv : Argument Vector
  */
int
main(int argc, char *argv[])
{
    /* create decoder context. */
    context_t **ctx;
    int ret = 0;
    /* save decode iterator number */
    int iterator_num = 0;
    int stress;
    int stats;
    void * error;

    /* get number of decoding streams */
    num_files = get_num_files(argc, argv);
    stream_stats = (fps_stats **)malloc(num_files * sizeof(fps_stats *));
    ctx = (context_t **)malloc(num_files * sizeof(context_t *));

    if (num_files == -1)
    {
        fprintf(stderr, "Error parsing commandline arguments\n");
        return -1;
    }

    argv+=2;

    do
    {
        /* set defaults for contexts */
        set_defaults (ctx,stream_stats);

        /* parse the arguments */
        if (parse_csv_args(ctx, argc-3, argv, num_files))
        {
            fprintf(stderr, "Error parsing commandline arguments\n");
            return -1;
        }

        stress = ctx[0]->stress_test;
        stats = ctx[0]->stats;
        for (int i = 0 ; i < num_files ; i++)
        {
            /* Spawn multiple decoding threads for multiple decoders. */
            pthread_create(&(ctx[i]->decode_thread), NULL, decode_proc, ctx[i]);
            char dec_output_plane[16] = "DecOutplane";
            string s = to_string(i);
            strcat(dec_output_plane, s.c_str());
            /* Name each spawned thread. */
            pthread_setname_np(ctx[i]->decode_thread, dec_output_plane);
        }

        for (int i = 0 ; i < num_files ; i++)
        {
            /* Wait for the decoding thread */
            pthread_join(ctx[i]->decode_thread, &error);
            if (*(int *)error != 0)
            {
                ret = *(int *)error;
            }
            free (error);
        }
        iterator_num++;
        if (stats)
        {
            /* Print the decoding stats for each stream */
            print_stats();
            for (int i = 0 ; i < num_files ; i++)
            {
                free (stream_stats[i]->filename);
                free (stream_stats[i]);
            }
        }
    } while ((stress != iterator_num));

    free (ctx);
    free (stream_stats);
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
