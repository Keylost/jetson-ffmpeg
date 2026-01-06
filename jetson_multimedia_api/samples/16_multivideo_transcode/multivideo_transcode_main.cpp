/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
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

#include <fstream>
#include <iostream>
#include <linux/videodev2.h>
#include <malloc.h>
#include <sstream>
#include <string.h>
#include <fcntl.h>
#include <poll.h>

#include "nvbuf_utils.h"
#include "NvUtils.h"
#include "multivideo_transcode.h"

using namespace std;

int num_files;
fps_stats **stream_stats;

/**
  * Abort on error.
  *
  * @param ctx : Transcoder context
  */
static void
abort(context_t *ctx)
{
    ctx->got_error = true;
    ctx->enc->abort();
    ctx->dec->abort();
}

/**
  * Initialise CRC Rec and creates CRC Table based on the polynomial.
  *
  * @param CrcPolynomial : CRC Polynomial values
  */
static
Crc* InitCrc(uint32_t CrcPolynomial)
{
    unsigned short int i;
    unsigned short int j;
    uint32_t tempcrc;
    Crc *phCrc;
    phCrc = (Crc*) malloc (sizeof(Crc));
    if (phCrc == NULL)
    {
        cerr << "Mem allocation failed for Init CRC" <<endl;
        return NULL;
    }

    memset (phCrc, 0, sizeof(Crc));

    for (i = 0; i <= 255; i++)
    {
        tempcrc = i;
        for (j = 8; j > 0; j--)
        {
            if (tempcrc & 1)
            {
                tempcrc = (tempcrc >> 1) ^ CrcPolynomial;
            }
            else
            {
                tempcrc >>= 1;
            }
        }
        phCrc->CRCTable[i] = tempcrc;
    }

    phCrc->CrcValue = 0;
    return phCrc;
}

/**
  * Calculates CRC of data provided in by buffer.
  *
  * @param *phCrc : bitstream CRC
  * @param buffer : process buffer
  * @param count  : bytes used
  */
static
void CalculateCrc(Crc *phCrc, unsigned char *buffer, uint32_t count)
{
    unsigned char *p;
    uint32_t temp1;
    uint32_t temp2;
    uint32_t crc = phCrc->CrcValue;
    uint32_t *CRCTable = phCrc->CRCTable;

    if (!count)
    {
        return;
    }

    p = (unsigned char *) buffer;
    while (count-- != 0)
    {
        temp1 = (crc >> 8) & 0x00FFFFFFL;
        temp2 = CRCTable[((uint32_t) crc ^ *p++) & 0xFF];
        crc = temp1 ^ temp2;
    }

    phCrc->CrcValue = crc;
}

/**
  * Closes CRC related handles.
  *
  * @param *phCrc : bitstream CRC
  */
static
void CloseCrc(Crc **phCrc)
{
    if (*phCrc)
    {
        free (*phCrc);
    }
}

static void
print_stats(int num_files)
{
    for ( int i = 0 ; i < num_files ; i++ )
    {
        cout << "******************* Instance "<< i <<"**********************" << endl;
        cout << "Stream = " << stream_stats[i]->filename << endl;
        cout << "Average FPS = " <<
            (((float) (stream_stats[i]->enc_data.total_processed_units - 1)) *
            1000000000 / TIMESPEC_DIFF_USEC(&stream_stats[i]->end_time,
                &stream_stats[i]->start_time)) << endl;
        cout << "Average latency(usec) = " <<
            MAX(stream_stats[i]->enc_data.average_latency_usec,
            stream_stats[i]->dec_data.average_latency_usec) << endl;

        cout << "*****************************************" << endl;
    }
}

/**
  * Report decoder input header error metadata.
  *
  * @param ctx             : Transcoder context
  * @param input_metadata  : Pointer to decoder input header error metadata struct
  */
static int
report_input_metadata(context_t *ctx, v4l2_ctrl_videodec_inputbuf_metadata *input_metadata)
{
    int ret = -1;
    uint32_t frame_num = ctx->dec->output_plane.getTotalDequeuedBuffers() - 1;

    /* NOTE: Bits represent types of error as defined with v4l2_videodec_input_error_type. */
    if (input_metadata->nBitStreamError & V4L2_DEC_ERROR_SPS)
    {
        cout << "Frame " << frame_num << " BitStreamError : ERROR_SPS " << endl;
    }
    else if (input_metadata->nBitStreamError & V4L2_DEC_ERROR_PPS)
    {
        cout << "Frame " << frame_num << " BitStreamError : ERROR_PPS " << endl;
    }
    else if (input_metadata->nBitStreamError & V4L2_DEC_ERROR_SLICE_HDR)
    {
        cout << "Frame " << frame_num << " BitStreamError : ERROR_SLICE_HDR " << endl;
    }
    else if (input_metadata->nBitStreamError & V4L2_DEC_ERROR_MISSING_REF_FRAME)
    {
        cout << "Frame " << frame_num << " BitStreamError : ERROR_MISSING_REF_FRAME " << endl;
    }
    else if (input_metadata->nBitStreamError & V4L2_DEC_ERROR_VPS)
    {
        cout << "Frame " << frame_num << " BitStreamError : ERROR_VPS " << endl;
    }
    else
    {
        cout << "Frame " << frame_num << " BitStreamError : ERROR_None " << endl;
    }
    ret = 0;
    return ret;
}

/**
  * Report decoder output metadata.
  *
  * @param ctx      : Transcoder context
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
            cout << "nActiveRefFrames = " << metadata->CodecParams.H264DecParams.dpbInfo.nActiveRefFrames << endl;
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
            cout << "nActiveRefFrames = " << metadata->CodecParams.HEVCDecParams.dpbInfo.nActiveRefFrames << endl;
        }

        if (metadata->FrameDecStats.DecodeError)
        {
            /* decoder error status metadata. */
            v4l2_ctrl_videodec_statusmetadata *dec_stats =
                &metadata->FrameDecStats;
            cout << "ErrorType="  << dec_stats->DecodeError << " Decoded MBs=" <<
                dec_stats->DecodedMBs << " Concealed MBs=" <<
                dec_stats->ConcealedMBs << endl;
        }
    }
    else
    {
        cout << "No valid metadata for frame" << endl;
    }
}

/**
  * Parse runtime command stream.
  *
  * @param ctx   : Transcoder context
  * @param id    : command obtained during runtime
  * @param value : Integer value obtained during runtime
  */
static int
get_next_parsed_pair(context_t *ctx, char *id, uint32_t *value)
{
    char charval;

    *ctx->runtime_params_str >> *id;
    if (ctx->runtime_params_str->eof())
    {
        return -1;
    }

    charval = ctx->runtime_params_str->peek();
    if (!IS_DIGIT(charval))
    {
        return -1;
    }

    *ctx->runtime_params_str >> *value;

    *ctx->runtime_params_str >> charval;
    if (ctx->runtime_params_str->eof())
    {
        return 0;
    }

    return charval;
}

/**
  * Set Runtime Parameters.
  *
  * @param ctx : Transcoder context
  */
static int
set_runtime_params(context_t *ctx)
{
    char charval;
    uint32_t intval;
    int ret, next;

    cout << "Frame " << ctx->next_param_change_frame <<
        ": Changing parameters" << endl;
    while (!ctx->runtime_params_str->eof())
    {
        next = get_next_parsed_pair(ctx, &charval, &intval);
        TEST_PARSE_ERROR(next < 0, err);
        switch (charval)
        {
            case 'b':
                if (ctx->ratecontrol == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR &&
                    ctx->peak_bitrate < intval)
                {
                    uint32_t peak_bitrate = 1.2f * intval;
                    cout << "Peak bitrate = " << peak_bitrate << endl;
                    ret = ctx->enc->setPeakBitrate(peak_bitrate);
                    if (ret < 0)
                    {
                        cerr << "Could not set encoder peakbitrate" << endl;
                        goto err;
                    }
                }
                cout << "Bitrate = " << intval << endl;
                ret = ctx->enc->setBitrate(intval);
                if (ret < 0)
                {
                    cerr << "Could not set encoder bitrate" << endl;
                    goto err;
                }
                break;
            case 'p':
                cout << "Peak bitrate = " << intval << endl;
                ret = ctx->enc->setPeakBitrate(intval);
                if (ret < 0)
                {
                    cerr << "Could not set encoder peakbitrate" << endl;
                    goto err;
                }
                break;
            case 'r':
            {
                int fps_num = intval;
                TEST_PARSE_ERROR(next != '/', err);

                ctx->runtime_params_str->seekg(-1, ios::cur);
                next = get_next_parsed_pair(ctx, &charval, &intval);
                TEST_PARSE_ERROR(next < 0, err);

                cout << "Framerate = " << fps_num << "/"  << intval << endl;

                ret = ctx->enc->setFrameRate(fps_num, intval);
                if (ret < 0)
                {
                    cerr << "Could not set framerate" << endl;
                    goto err;
                }
                break;
            }
            case 'i':
                if (intval > 0)
                {
                    ctx->enc->forceIDR();
                    cout << "Forcing IDR" << endl;
                }
                break;
            default:
                TEST_PARSE_ERROR(true, err);
        }
        switch (next)
        {
            case 0:
                delete ctx->runtime_params_str;
                ctx->runtime_params_str = NULL;
                return 0;
            case '#':
                return 0;
            case ',':
                break;
            default:
                break;
        }
    }
    return 0;
err:
    cerr << "Skipping further runtime parameter changes" <<endl;
    delete ctx->runtime_params_str;
    ctx->runtime_params_str = NULL;
    return -1;
}

/**
  * Read the input chunks for h264/H265.
  *
  * @param stream : Input stream
  * @param buffer : NvBuffer pointer
  */
static int
read_decoder_input_chunk(context_t *ctx, NvBuffer * buffer)
{
    ifstream *stream = ctx->in_file;
    /* Length is the size of the buffer in bytes */
    streamsize bytes_to_read = MIN(CHUNK_SIZE, buffer->planes[0].length);

    stream->read((char *) buffer->planes[0].data, bytes_to_read);
    /* NOTE: It is necessary to set bytesused properly, so that decoder knows how
             many bytes in the buffer are valid. */
    buffer->planes[0].bytesused = stream->gcount();
    if (buffer->planes[0].bytesused == 0)
    {
        stream->clear();
        stream->seekg(0,stream->beg);

        if (ctx->seek_mode)
        {
            ctx->iterator_num++;
            if (ctx->iterator_num < ctx->num_iterations)
            {
                stream->read((char *) buffer->planes[0].data, bytes_to_read);
                buffer->planes[0].bytesused = stream->gcount();
            }
        }
    }
    return 0;
}

/**
  * Read the input chunks for Vp8/Vp9 decoder.
  *
  * @param ctx    : Transcoder context
  * @param buffer : NvBuffer pointer
  */
static int
read_vpx_decoder_input_chunk(context_t *ctx, NvBuffer * buffer)
{
    ifstream *stream = ctx->in_file;
    int Framesize;
    unsigned char *bitstreambuffer = (unsigned char *)buffer->planes[0].data;
    if (ctx->dec_vp9_file_header_flag == 0)
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
        ctx->dec_vp9_file_header_flag = 1;
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
  * Read the input NAL unit for h264/H265.
  *
  * @param stream            : Input stream
  * @param buffer            : NvBuffer pointer
  * @param parse_buffer      : parse buffer pointer
  * @param parse_buffer_size : chunk size
  * @param ctx               : Transcoder context
  */
static int
read_decoder_input_nalu(context_t *ctx, NvBuffer * buffer,
        char *parse_buffer, streamsize parse_buffer_size)
{
    ifstream *stream = ctx->in_file;
    /* Length is the size of the buffer in bytes. */
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
        stream->clear();
        stream->seekg(0,stream->beg);

        if (ctx->seek_mode)
        {
            ctx->iterator_num++;
            if (ctx->iterator_num < ctx->num_iterations)
            {
                stream->read(parse_buffer, parse_buffer_size);
                bytes_read = stream->gcount();
            }
        }
        else
            return buffer->planes[0].bytesused = 0;
    }

    /* Find the first NAL unit in the buffer. */
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

    /* Reached end of buffer but could not find NAL unit. */
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
        if (ctx->decoder_pixfmt == V4L2_PIX_FMT_H264)
        {
            if ((IS_H264_NAL_CODED_SLICE(stream_ptr)) ||
                (IS_H264_NAL_CODED_SLICE_IDR(stream_ptr)))
            {
                ctx->flag_copyts = true;
            }
            else
            {
                ctx->flag_copyts = false;
            }
        }
        else if (ctx->decoder_pixfmt == V4L2_PIX_FMT_H265)
        {
            h265_nal_unit_type = GET_H265_NAL_UNIT_TYPE(stream_ptr);

            if ((h265_nal_unit_type >= HEVC_NUT_TRAIL_N && h265_nal_unit_type <= HEVC_NUT_RASL_R) ||
            (h265_nal_unit_type >= HEVC_NUT_BLA_W_LP && h265_nal_unit_type <= HEVC_NUT_CRA_NUT))
            {
                ctx->flag_copyts = true;
            }
            else
            {
                ctx->flag_copyts = false;
            }
        }
    }

    /* Copy bytes till the next NAL unit is found. */
    while ((stream_ptr - parse_buffer) < (bytes_read - 3))
    {
        if (IS_NAL_UNIT_START(stream_ptr) || IS_NAL_UNIT_START1(stream_ptr))
        {
            streamsize seekto = stream_initial_pos +
                    (stream_ptr - parse_buffer);
            if (stream->eof())
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

    /* Reached end of buffer but could not find NAL unit. */
    cerr << "Could not read nal unit from file. EOF or file corrupted"
            << endl;
    return -1;
}

/**
  * Write transcoded frame data.
  *
  * @param stream : output stream
  * @param buffer : output nvbuffer
  */
static int
write_transcoder_output_frame(ofstream * stream, NvBuffer * buffer)
{
    stream->write((char *) buffer->planes[0].data, buffer->planes[0].bytesused);
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
encoder_capture_plane_dq_callback(struct v4l2_buffer *v4l2_buf, NvBuffer * buffer,
                                  NvBuffer * shared_buffer, void *arg)
{
    context_t *ctx = (context_t *) arg;
    NvVideoEncoder *enc = ctx->enc;
    pthread_setname_np(pthread_self(), "EncCapPlane");
    uint32_t frame_num = ctx->enc->capture_plane.getTotalDequeuedBuffers() - 1;
    static uint32_t num_encoded_frames = 1;
    struct v4l2_event ev;
    int ret = 0;

    if (v4l2_buf == NULL)
    {
        cout << "Error while dequeing buffer from output plane" << endl;
        abort(ctx);
        return false;
    }

    if (ctx->b_use_enc_cmd)
    {
        if (v4l2_buf->flags & V4L2_BUF_FLAG_LAST)
        {
            memset(&ev,0,sizeof(struct v4l2_event));
            ret = ctx->enc->dqEvent(ev,1000);
            if (ret < 0)
            {
                cout << "Error in dqEvent" << endl;
            }
            if (ev.type == V4L2_EVENT_EOS)
            {
                return false;
            }
        }
    }

    /* Received EOS from encoder. Stop dqthread. */
    if (buffer->planes[0].bytesused == 0)
    {
        cout << "Got 0 size buffer in capture \n";
        return false;
    }

    /* Computing CRC with each frame */
    if (ctx->pBitStreamCrc)
    {
        CalculateCrc (ctx->pBitStreamCrc, buffer->planes[0].data, buffer->planes[0].bytesused);
    }

    if (!ctx->stats)
    {
        write_transcoder_output_frame(ctx->out_file, buffer);
    }

    num_encoded_frames++;

    if (ctx->enc_report_metadata)
    {
        v4l2_ctrl_videoenc_outputbuf_metadata enc_metadata;
        if (ctx->enc->getMetadata(v4l2_buf->index, enc_metadata) == 0)
        {
            cout << "Frame " << frame_num <<
                ": isKeyFrame=" << (int) enc_metadata.KeyFrame <<
                " AvgQP=" << enc_metadata.AvgQP <<
                " MinQP=" << enc_metadata.FrameMinQP <<
                " MaxQP=" << enc_metadata.FrameMaxQP <<
                " EncodedBits=" << enc_metadata.EncodedFrameBits <<
                endl;
        }
    }
    if (ctx->dump_mv)
    {
        /* Get motion vector parameters of the frames from encoder */
        v4l2_ctrl_videoenc_outputbuf_metadata_MV enc_mv_metadata;
        if (ctx->enc->getMotionVectors(v4l2_buf->index, enc_mv_metadata) == 0)
        {
            uint32_t numMVs = enc_mv_metadata.bufSize / sizeof(MVInfo);
            MVInfo *pInfo = enc_mv_metadata.pMVInfo;

            cout << "Frame " << frame_num << ": Num MVs=" << numMVs << endl;

            for (uint32_t i = 0; i < numMVs; i++, pInfo++)
            {
                cout << i << ": mv_x=" << pInfo->mv_x <<
                    " mv_y=" << pInfo->mv_y <<
                    " weight=" << pInfo->weight <<
                    endl;
            }
        }
    }

    /* encoder qbuffer for capture plane */
    if (enc->capture_plane.qBuffer(*v4l2_buf, NULL) < 0)
    {
        cerr << "Error while Qing buffer at capture plane" << endl;
        abort(ctx);
        return false;
    }

    return true;
}

/**
  * Get the next runtime parameters change for frame.
  *
  * @param ctx : Transcoder context
  */
static int
get_next_runtime_param_change_frame(context_t *ctx)
{
    char charval;
    int ret;

    ret = get_next_parsed_pair(ctx, &charval, &ctx->next_param_change_frame);
    if (ret == 0)
    {
        return 0;
    }

    TEST_PARSE_ERROR((ret != ';' && ret != ',') || charval != 'f', err);

    return 0;

err:
    cerr << "Skipping further runtime parameter changes" <<endl;
    delete ctx->runtime_params_str;
    ctx->runtime_params_str = NULL;
    return -1;
}

/**
  * Set transcoder context defaults values.
  *
  * @param ctx          : Transcoder context
  * @param fps_stats    : structure for stats
  * @param files        : Number of input files
  */
static void
set_defaults(context_t ** ctx, fps_stats **stream_stats, int files )
{
    for ( int i = 0 ; i < files ; i++ )
    {
        ctx[i] = (context_t *) malloc(sizeof(context_t));
        stream_stats[i] = (fps_stats *)malloc(sizeof(fps_stats));
        memset(ctx[i], 0, sizeof(context_t));
        memset(stream_stats[i], 0 , sizeof(stream_stats));
        ctx[i]->thread_num = i;
        ctx[i]->in_file_path = NULL;
        ctx[i]->out_file_path = NULL;
        ctx[i]->dec_output_plane_mem_type = V4L2_MEMORY_MMAP;
        ctx[i]->dec_capture_plane_mem_type = V4L2_MEMORY_DMABUF;
        ctx[i]->dec_vp9_file_header_flag = 0;
        ctx[i]->dec_vp8_file_header_flag = 0;
        ctx[i]->raw_pixfmt = V4L2_PIX_FMT_YUV420M;
        ctx[i]->bitrate = 4 * 1024 * 1024;
        ctx[i]->peak_bitrate = 0;
        ctx[i]->profile = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
        ctx[i]->ratecontrol = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
        ctx[i]->iframe_interval = 30;
        ctx[i]->bnoIframe = false;
        ctx[i]->enable_lossless = false;
        ctx[i]->nH264FrameNumBits = 0;
        ctx[i]->nH265PocLsbBits = 0;
        ctx[i]->idr_interval = 256;
        ctx[i]->level = -1;
        ctx[i]->fps_n = 30;
        ctx[i]->fps_d = 1;
        ctx[i]->num_b_frames = (uint32_t) -1;
        ctx[i]->nMinQpI = (uint32_t)QP_RETAIN_VAL;
        ctx[i]->nMaxQpI = (uint32_t)QP_RETAIN_VAL;
        ctx[i]->nMinQpP = (uint32_t)QP_RETAIN_VAL;
        ctx[i]->nMaxQpP = (uint32_t)QP_RETAIN_VAL;
        ctx[i]->nMinQpB = (uint32_t)QP_RETAIN_VAL;
        ctx[i]->nMaxQpB = (uint32_t)QP_RETAIN_VAL;
        ctx[i]->use_gold_crc = false;
        ctx[i]->pBitStreamCrc = NULL;
        ctx[i]->dec_input_metadata = false;
        ctx[i]->stats = false;
        ctx[i]->seek_mode = false;
        ctx[i]->iterator_num = 0;
        ctx[i]->num_iterations = 1;
        ctx[i]->enc_output_memory_type = V4L2_MEMORY_DMABUF;
        ctx[i]->enc_capture_memory_type = V4L2_MEMORY_MMAP;
        ctx[i]->cs = V4L2_COLORSPACE_SMPTE170M;
        ctx[i]->copy_timestamp = false;
        ctx[i]->start_ts = 0;
        ctx[i]->max_perf = 0;
        ctx[i]->blocking_mode = 1;
        ctx[i]->num_output_buffers = NUM_ENCODER_OUTPUT_BUFFERS;
        ctx[i]->num_frames_to_encode = -1;
        ctx[i]->poc_type = 0;
        ctx[i]->stop_refill = 0;
    }
}

/**
  * To refill buffers from encoder output plane to
  * to decoder capture plane
  *
  * @param arg          : pointer to transcoder context
  */
static void *
buffer_refil(void *arg)
{
    context_t *ctx = (context_t *) arg;
    NvVideoDecoder *dec = ctx->dec;
    NvVideoEncoder *enc = ctx->enc;
    bool sent_eos = false;

    /* Set thread name for decoder Capture Plane thread. */
    pthread_setname_np(ctx->buffer_refill, "BufferRefill");

    while(!ctx->stop_refill || !sent_eos)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;

        /* dequeue buffer from encoder output plane */
        if (enc->output_plane.dqBuffer(v4l2_buf, &buffer, NULL, 10) < 0)
        {
            cerr << "ERROR while DQing buffer at output plane" << endl;
            abort(ctx);
            break;
        }

        if (!ctx->stop_refill)
        {
            if (ctx->dec_capture_plane_mem_type == V4L2_MEMORY_DMABUF)
            {
                buffer->planes[0].fd = ctx->dmabuff_fd[v4l2_buf.index];
                v4l2_buf.m.planes[0].m.fd = ctx->dmabuff_fd[v4l2_buf.index];
            }

            if (dec->capture_plane.qBuffer(v4l2_buf, NULL) < 0)
            {
                abort(ctx);
                cerr <<
                    "Error while queueing buffer at decoder capture plane"
                    << endl;
                break;
            }
        }
        else
        {
            /* Send size 0 buffer to encoder as EoS */
            buffer = enc->output_plane.getNthBuffer(v4l2_buf.index);
            v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

            if (ctx->enc_output_memory_type == V4L2_MEMORY_DMABUF)
            {
                for (uint32_t i = 0 ; i < buffer->n_planes ; i++)
                {
                    buffer->planes[i].fd = ctx->dmabuff_fd[v4l2_buf.index];
                    v4l2_buf.m.planes[i].m.fd = buffer->planes[i].fd;
                    buffer->planes[i].bytesused = 0;
                    v4l2_buf.m.planes[i].bytesused = 0;
                }
            }

            /* Enqueue a size 0 buffer in encoder */
            if (ctx->enc->output_plane.qBuffer(v4l2_buf, NULL) < 0)
            {
                abort(ctx);
                cerr <<
                    "Error while queueing buffer at encoder output plane"
                    << endl;
            }
            sent_eos = true;
        }
    }

    return NULL;
}

/**
  * Query and Set Capture plane.
  *
  * @param ctx : Transcoder context
  */
static void
query_and_set_capture(context_t * ctx)
{
    NvVideoDecoder *dec = ctx->dec;
    NvVideoEncoder *enc = ctx->enc;
    struct v4l2_format format;
    struct v4l2_crop crop;
    int32_t min_dec_capture_buffers;
    int ret = 0;
    int error = 0;
    NvBufferCreateParams cParams = {0};

    /* Get capture plane format from the decoder.
       This may change after resolution change event.
       Refer ioctl VIDIOC_G_FMT */
    ret = dec->capture_plane.getFormat(format);
    TEST_ERROR(ret < 0,
               "Error: Could not get format from decoder capture plane", error);

     ret = dec->capture_plane.getCrop(crop);
    TEST_ERROR(ret < 0,
               "Error: Could not get crop from decoder capture plane", error);

    cout << "Video Resolution: " << crop.c.width << "x" << crop.c.height;

    /* deinitPlane unmaps the buffers and calls REQBUFS with count 0 */
    dec->capture_plane.deinitPlane();

    enc->output_plane.deinitPlane();

    enc->capture_plane.deinitPlane();

    ctx->height = crop.c.height;
    ctx->width = crop.c.width;
    ctx->raw_pixfmt = format.fmt.pix_mp.pixelformat;
    /* Not necessary to call VIDIOC_S_FMT on decoder capture plane.
       But decoder setCapturePlaneFormat function updates the class variables */
    ret = dec->setCapturePlaneFormat(ctx->raw_pixfmt,
                                     ctx->width,
                                     ctx->height);
    TEST_ERROR(ret < 0, "Error in setting decoder capture plane format", error);

    /* Get the minimum buffers which have to be requested on the capture plane. */
    ret = dec->getMinimumCapturePlaneBuffers(min_dec_capture_buffers);
    TEST_ERROR(ret < 0,
               "Error while getting value of minimum capture plane buffers",
               error);

    /* Request, Query and export decoder capture plane buffers.
       Refer ioctl VIDIOC_REQBUFS, VIDIOC_QUERYBUF and VIDIOC_EXPBUF */
    if (ctx->dec_capture_plane_mem_type == V4L2_MEMORY_DMABUF)
    {
        /* Set colorformats for relevant colorspaces. */
        switch(format.fmt.pix_mp.colorspace)
        {
            case V4L2_COLORSPACE_SMPTE170M:
                if (format.fmt.pix_mp.quantization == V4L2_QUANTIZATION_DEFAULT)
                {
                    cout << "Decoder colorspace ITU-R BT.601 with standard range luma (16-235)" << endl;
                    cParams.colorFormat = NvBufferColorFormat_NV12;
                }
                else
                {
                    cout << "Decoder colorspace ITU-R BT.601 with extended range luma (0-255)" << endl;
                    cParams.colorFormat = NvBufferColorFormat_NV12_ER;
                }
                break;
            case V4L2_COLORSPACE_REC709:
                if (format.fmt.pix_mp.quantization == V4L2_QUANTIZATION_DEFAULT)
                {
                    cout << "Decoder colorspace ITU-R BT.709 with standard range luma (16-235)" << endl;
                    cParams.colorFormat = NvBufferColorFormat_NV12_709;
                }
                else
                {
                    cout << "Decoder colorspace ITU-R BT.709 with extended range luma (0-255)" << endl;
                    cParams.colorFormat = NvBufferColorFormat_NV12_709_ER;
                }
                break;
            case V4L2_COLORSPACE_BT2020:
                {
                    cout << "Decoder colorspace ITU-R BT.2020" << endl;
                    cParams.colorFormat = NvBufferColorFormat_NV12_2020;
                }
                break;
            default:
                cout << "supported colorspace details not available, use default" << endl;
                if (format.fmt.pix_mp.quantization == V4L2_QUANTIZATION_DEFAULT)
                {
                    cout << "Decoder colorspace ITU-R BT.601 with standard range luma (16-235)" << endl;
                    cParams.colorFormat = NvBufferColorFormat_NV12;
                }
                else
                {
                    cout << "Decoder colorspace ITU-R BT.601 with extended range luma (0-255)" << endl;
                    cParams.colorFormat = NvBufferColorFormat_NV12_ER;
                }
                break;
        }

        ctx->num_cap_buffers = min_dec_capture_buffers + ctx->extra_cap_plane_buffer;

        /* Create decoder capture plane buffers. */
        for (int index = 0; index < ctx->num_cap_buffers; index++)
        {
            cParams.width = ctx->width;
            cParams.height = ctx->height;
            cParams.layout = NvBufferLayout_BlockLinear;
            cParams.payloadType = NvBufferPayload_SurfArray;
            cParams.nvbuf_tag = NvBufferTag_VIDEO_DEC;
            ret = NvBufferCreateEx(&ctx->dmabuff_fd[index], &cParams);
            TEST_ERROR(ret < 0, "Failed to create buffers", error);
        }

        /* Request buffers on decoder capture plane.
           Refer ioctl VIDIOC_REQBUFS */
        ret = dec->capture_plane.reqbufs(ctx->dec_capture_plane_mem_type, ctx->num_cap_buffers);
            TEST_ERROR(ret, "Error in request buffers on capture plane", error);
    }

    ret = enc->setCapturePlaneFormat(ctx->encoder_pixfmt, ctx->width,
                                         ctx->height, 2 * 1024 * 1024);
    TEST_ERROR(ret < 0, "Could not set encoder capture plane format", error);

    ret = enc->setOutputPlaneFormat(ctx->raw_pixfmt, ctx->width,
                                      ctx->height);
    TEST_ERROR(ret < 0, "Could not set encoder output plane format", error);

    ret = enc->setBitrate(ctx->bitrate);
    TEST_ERROR(ret < 0, "Could not set encoder bitrate", error);

    if (ctx->encoder_pixfmt == V4L2_PIX_FMT_H264)
    {
        /* Set encoder profile for H264 format */
        ret = enc->setProfile(ctx->profile);
        TEST_ERROR(ret < 0, "Could not set encoder profile", error);

        if (ctx->level == (uint32_t)-1)
        {
            ctx->level = (uint32_t)V4L2_MPEG_VIDEO_H264_LEVEL_5_1;
        }

        /* Set encoder level for H264 format */
        ret = enc->setLevel(ctx->level);
        TEST_ERROR(ret < 0, "Could not set encoder level", error);
    }
    else if (ctx->encoder_pixfmt == V4L2_PIX_FMT_H265)
    {
        /* Set encoder profile for HEVC format */
        ret = enc->setProfile(ctx->profile);
        TEST_ERROR(ret < 0, "Could not set encoder profile", error);

        if (ctx->level != (uint32_t)-1)
        {
            /* Set encoder level for HEVC format */
            ret = enc->setLevel(ctx->level);
            TEST_ERROR(ret < 0, "Could not set encoder level", error);
        }
    }

    if (ctx->enable_lossless)
    {
        /* Set constant qp configuration for lossless encoding enabled */
        ret = enc->setConstantQp(0);
        TEST_ERROR(ret < 0, "Could not set encoder constant qp=0", error);
    }
    else
    {
        /* Set rate control mode for encoder */
        ret = enc->setRateControlMode(ctx->ratecontrol);
        TEST_ERROR(ret < 0, "Could not set encoder rate control mode", error);
        if (ctx->ratecontrol == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR)
        {
            uint32_t peak_bitrate;
            if (ctx->peak_bitrate < ctx->bitrate)
            {
                peak_bitrate = 1.2f * ctx->bitrate;
            }
            else
            {
                peak_bitrate = ctx->peak_bitrate;
            }
            /* Set peak bitrate value for variable bitrate mode for encoder */
            ret = enc->setPeakBitrate(peak_bitrate);
            TEST_ERROR(ret < 0, "Could not set encoder peak bitrate", error);
        }
    }

    if (ctx->poc_type)
    {
        ret = enc->setPocType(ctx->poc_type);
        TEST_ERROR(ret < 0, "Could not set Picture Order Count value", error);
    }

    /* Set IDR frame interval for encoder */
    ret = enc->setIDRInterval(ctx->idr_interval);
    TEST_ERROR(ret < 0, "Could not set encoder IDR interval", error);

    /* Set I frame interval for encoder */
    ret = enc->setIFrameInterval(ctx->iframe_interval);
    TEST_ERROR(ret < 0, "Could not set encoder I-Frame interval", error);

    /* Set framerate for encoder */
    ret = enc->setFrameRate(ctx->fps_n, ctx->fps_d);
    TEST_ERROR(ret < 0, "Could not set framerate", error);

    if (ctx->temporal_tradeoff_level)
    {
        /* Set temporal tradeoff level value for encoder */
        ret = enc->setTemporalTradeoff(ctx->temporal_tradeoff_level);
        TEST_ERROR(ret < 0, "Could not set temporal tradeoff level", error);
    }

    if (ctx->slice_length)
    {
        /* Set slice length value for encoder */
        ret = enc->setSliceLength(ctx->slice_length_type,
                ctx->slice_length);
        TEST_ERROR(ret < 0, "Could not set slice length params", error);
    }

    if (ctx->hw_preset_type)
    {
        /* Set hardware preset value for encoder */
        ret = enc->setHWPresetType(ctx->hw_preset_type);
        TEST_ERROR(ret < 0, "Could not set encoder HW Preset Type", error);
    }

    if (ctx->virtual_buffer_size)
    {
        /* Set virtual buffer size value for encoder */
        ret = enc->setVirtualBufferSize(ctx->virtual_buffer_size);
        TEST_ERROR(ret < 0, "Could not set virtual buffer size", error);
    }

    if (ctx->num_reference_frames)
    {
        /* Set number of reference frame configuration value for encoder */
        ret = enc->setNumReferenceFrames(ctx->num_reference_frames);
        TEST_ERROR(ret < 0, "Could not set num reference frames", error);
    }

    if (ctx->slice_intrarefresh_interval)
    {
        /* Set slice intra refresh interval value for encoder */
        ret = enc->setSliceIntrarefresh(ctx->slice_intrarefresh_interval);
        TEST_ERROR(ret < 0, "Could not set slice intrarefresh interval", error);
    }

    if (ctx->insert_sps_pps_at_idr)
    {
        /* Enable insert of SPSPPS at IDR frames */
        ret = enc->setInsertSpsPpsAtIdrEnabled(true);
        TEST_ERROR(ret < 0, "Could not set insertSPSPPSAtIDR", error);
    }

    if (ctx->disable_cabac)
    {
        /* Disable CABAC entropy encoding */
        ret = enc->setCABAC(false);
        TEST_ERROR(ret < 0, "Could not set disable CABAC", error);
    }

    if (ctx->insert_vui)
    {
        /* Enable insert of VUI parameters */
        ret = enc->setInsertVuiEnabled(true);
        TEST_ERROR(ret < 0, "Could not set insertVUI", error);
    }

    if (ctx->enable_extended_colorformat)
    {
        /* Enable extnded colorformat for encoder */
        ret = enc->setExtendedColorFormat(true);
        TEST_ERROR(ret < 0, "Could not set extended color format", error);
    }

    if (ctx->insert_aud)
    {
        /* Enable insert of AUD parameters */
        ret = enc->setInsertAudEnabled(true);
        TEST_ERROR(ret < 0, "Could not set insertAUD", error);
    }

    if (ctx->alliframes)
    {
        /* Enable all I-frame encode */
        ret = enc->setAlliFramesEncode(true);
        TEST_ERROR(ret < 0, "Could not set Alliframes encoding", error);
    }

    if (ctx->num_b_frames != (uint32_t) -1)
    {
        /* Set number of B-frames to to be used by encoder */
        ret = enc->setNumBFrames(ctx->num_b_frames);
        TEST_ERROR(ret < 0, "Could not set number of B Frames", error);
    }

    if ((ctx->nMinQpI != (uint32_t)QP_RETAIN_VAL) ||
        (ctx->nMaxQpI != (uint32_t)QP_RETAIN_VAL) ||
        (ctx->nMinQpP != (uint32_t)QP_RETAIN_VAL) ||
        (ctx->nMaxQpP != (uint32_t)QP_RETAIN_VAL) ||
        (ctx->nMinQpB != (uint32_t)QP_RETAIN_VAL) ||
        (ctx->nMaxQpB != (uint32_t)QP_RETAIN_VAL))
    {
        /* Set Min & Max qp range values for I/P/B-frames to be used by encoder */
        ret = enc->setQpRange(ctx->nMinQpI, ctx->nMaxQpI, ctx->nMinQpP,
                ctx->nMaxQpP, ctx->nMinQpB, ctx->nMaxQpB);
        TEST_ERROR(ret < 0, "Could not set quantization parameters", error);
    }

    if (ctx->max_perf)
    {
        /* Enable maximum performance mode by disabling internal DFS logic.
           NOTE: This enables encoder to run at max clocks */
        ret = enc->setMaxPerfMode(ctx->max_perf);
        TEST_ERROR(ret < 0, "Error while setting encoder to max perf", error);
    }

    if (ctx->dump_mv)
    {
        /* Enable dumping of motion vectors report from encoder */
        ret = enc->enableMotionVectorReporting();
        TEST_ERROR(ret < 0, "Could not enable motion vector reporting", error);
    }

    if (ctx->bnoIframe) {
        ctx->iframe_interval = ((1<<31) + 1); /* TODO: how can we do this properly */
        ret = enc->setIFrameInterval(ctx->iframe_interval);
        TEST_ERROR(ret < 0, "Could not set encoder I-Frame interval", error);
    }

    if (ctx->encoder_pixfmt == V4L2_PIX_FMT_H264)
    {
        /* Set encoder profile for H264 format */
        ret = enc->setProfile(ctx->profile);
        TEST_ERROR(ret < 0, "Could not set encoder profile", error);

        if (ctx->level == (uint32_t)-1)
        {
            ctx->level = (uint32_t)V4L2_MPEG_VIDEO_H264_LEVEL_5_1;
        }

        /* Set encoder level for H264 format */
        ret = enc->setLevel(ctx->level);
        TEST_ERROR(ret < 0, "Could not set encoder level", error);
    }
    else if (ctx->encoder_pixfmt == V4L2_PIX_FMT_H265)
    {
        /* Set encoder profile for HEVC format */
        ret = enc->setProfile(ctx->profile);
        TEST_ERROR(ret < 0, "Could not set encoder profile", error);

        if (ctx->level != (uint32_t)-1)
        {
            /* Set encoder level for HEVC format */
            ret = enc->setLevel(ctx->level);
            TEST_ERROR(ret < 0, "Could not set encoder level", error);
        }
    }

     /* Set IDR frame interval for encoder */
    ret = enc->setIDRInterval(ctx->idr_interval);
    TEST_ERROR(ret < 0, "Could not set encoder IDR interval", error);

    /* Set I frame interval for encoder */
    ret = enc->setIFrameInterval(ctx->iframe_interval);
    TEST_ERROR(ret < 0, "Could not set encoder I-Frame interval", error);

    /* Set framerate for encoder */
    ret = enc->setFrameRate(ctx->fps_n, ctx->fps_d);
    TEST_ERROR(ret < 0, "Could not set framerate", error);

    /*  Set encoder output plane */
    ret = enc->output_plane.reqbufs(ctx->enc_output_memory_type, ctx->num_cap_buffers);
    TEST_ERROR(ret < 0,"reqbufs failed for output plane V4L2_MEMORY_DMABUF", error);

     ret = enc->capture_plane.setupPlane(ctx->enc_capture_memory_type, ctx->num_output_buffers,
        true, false);
    TEST_ERROR(ret < 0, "Could not setup capture plane", error);

    /* Subscibe for End Of Stream event */
    ret = enc->subscribeEvent(V4L2_EVENT_EOS,0,0);
    TEST_ERROR(ret < 0, "Could not subscribe EOS event", error);

    /* Decoder capture plane STREAMON.
       Refer ioctl VIDIOC_STREAMON */

    ret = dec->capture_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in decoder capture plane streamon", error);

     /* set encoder output plane STREAMON */
    ret = enc->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in output plane streamon", error);

    /* set encoder capture plane STREAMON */
    ret = enc->capture_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in capture plane streamon", error);

    /* Set encoder capture plane dq thread callback for blocking io mode */
    enc->capture_plane.setDQThreadCallback(encoder_capture_plane_dq_callback);

    /* startDQThread starts a thread internally which calls the
       encoder_capture_plane_dq_callback whenever a buffer is dequeued
       on the plane */
    enc->capture_plane.startDQThread(ctx);

    pthread_create(&ctx->buffer_refill, NULL, buffer_refil, ctx);

    /* Enqueue all the empty encoder capture plane buffers. */
    for (uint32_t i = 0; i < enc->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        ret = enc->capture_plane.qBuffer(v4l2_buf, NULL);
        TEST_ERROR(ret < 0, "Error while queueing buffer at capture plane", error);

    }

    /* Enqueue all the empty decoder capture plane buffers. */
    for (uint32_t i = 0; i < dec->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        v4l2_buf.memory = ctx->dec_capture_plane_mem_type;
        if (ctx->dec_capture_plane_mem_type == V4L2_MEMORY_DMABUF)
        {
            v4l2_buf.m.planes[0].m.fd = ctx->dmabuff_fd[i];
        }
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
  * Decoder capture thread loop function.
  *
  * @param args : pointer to transcoder
  */
static void *
dec_capture_loop_fcn(void *arg)
{
    context_t *ctx = (context_t *) arg;
    NvVideoDecoder *dec = ctx->dec;
    NvVideoEncoder *enc = ctx->enc;
    struct v4l2_event ev;
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[MAX_PLANES];
    NvBuffer *buffer;
    NvBufferParams params;
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

    if (!ctx->got_error)
    {
        query_and_set_capture(ctx);
    }

    while (!(ctx->got_error || dec->isInError() || ctx->got_eos))
    {
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
            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            memset(planes, 0, sizeof(planes));
            v4l2_buf.m.planes = planes;

            /* Dequeue a filled buffer. */

            if (dec->capture_plane.dqBuffer(v4l2_buf, &buffer, NULL, 0))
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

            if (ctx->dec_enable_metadata)
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

            buffer = enc->output_plane.getNthBuffer(v4l2_buf.index);
            v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

            ret = NvBufferGetParams(ctx->dmabuff_fd[v4l2_buf.index], &params);
            if (ret < 0)
            {
                abort(ctx);
                cerr << "Error while calling NvBufferGetParams" << endl;
                break;
            }

            if (ctx->enc_output_memory_type == V4L2_MEMORY_DMABUF)
            {
                for (uint32_t i = 0 ; i < buffer->n_planes ; i++)
                {
                    buffer->planes[i].fd = ctx->dmabuff_fd[v4l2_buf.index];
                    v4l2_buf.m.planes[i].m.fd = buffer->planes[i].fd;
                    buffer->planes[i].mem_offset = params.offset[i];
                    buffer->planes[i].bytesused = buffer->planes[i].fmt.stride * buffer->planes[i].fmt.height;
                    v4l2_buf.m.planes[i].bytesused = buffer->planes[i].bytesused;
                }
            }

            if (ctx->runtime_params_str &&
                (ctx->enc->output_plane.getTotalQueuedBuffers() ==
                    ctx->next_param_change_frame))
            {
                /* Set runtime configuration parameters */
                set_runtime_params(ctx);
                if (ctx->runtime_params_str)
                {
                    get_next_runtime_param_change_frame(ctx);
                }
            }

            /* Encoder supported input metadata specific configurations */
            if (ctx->dec_input_metadata)
            {
                v4l2_ctrl_videoenc_input_metadata VEnc_imeta_param;
                VEnc_imeta_param.flag = 0;

                if (VEnc_imeta_param.flag)
                {
                    /* Set encoder input metadatas */
                    ctx->enc->SetInputMetaParams(v4l2_buf.index, VEnc_imeta_param);
                    v4l2_buf.reserved2 = v4l2_buf.index;
                }
            }

            if (ctx->copy_timestamp)
            {
                v4l2_buf.flags |= V4L2_BUF_FLAG_TIMESTAMP_COPY;
                ctx->timestamp += ctx->timestampincr;
                v4l2_buf.timestamp.tv_sec = ctx->timestamp / (MICROSECOND_UNIT);
                v4l2_buf.timestamp.tv_usec = ctx->timestamp % (MICROSECOND_UNIT);
            }

            if (ctx->enc->output_plane.qBuffer(v4l2_buf, NULL) < 0)
            {
                abort(ctx);
                cerr <<
                    "Error while queueing buffer at decoder capture plane"
                    << endl;
                break;
            }

        }
    }

    ctx->stop_refill=1;

    pthread_join(ctx->buffer_refill, NULL);

    cout << "Exiting decoder capture loop thread" << endl;
    return NULL;
}

/**
  * transcode processing function for blocking mode.
  *
  * @param ctx               : Transcoder context
  * @param eos               : end of stream
  * @param current_file      : current file
  * @param current_loop      : iterator count
  * @param nalu_parse_buffer : input parsed nal unit
  */
static bool transcoder_proc_blocking(context_t &ctx, bool eos, char *nalu_parse_buffer)
{
    bool allow_DQ = true;
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

        v4l2_buf.m.planes = planes;

        /* dequeue a buffer for output plane. */
        if (allow_DQ)
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

        if ((v4l2_buf.flags & V4L2_BUF_FLAG_ERROR) && ctx.dec_input_metadata)
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
                (ctx.decoder_pixfmt == V4L2_PIX_FMT_H265))
        {
            if (ctx.input_nalu)
            {
                /* read the input nal unit. */
                read_decoder_input_nalu(&ctx, buffer, nalu_parse_buffer,
                        CHUNK_SIZE);
            }
            else
            {
                /* read the input chunks. */
                read_decoder_input_chunk(&ctx, buffer);
            }
        }

        if (ctx.decoder_pixfmt == V4L2_PIX_FMT_VP9 || ctx.decoder_pixfmt == V4L2_PIX_FMT_VP8)
        {
            /* read the input chunks. */
            ret = read_vpx_decoder_input_chunk(&ctx, buffer);
            if (ret != 0)
            {
                cerr << "Couldn't read chunk" << endl;
            }
        }
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;

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
  * Transcode processing function.
  *
  * @param ctx  : Transcoder context
  */
static void *
transcode_proc(void * p_ctx)
{
    context_t ctx = *(context_t *)p_ctx;
    int ret = 0;
    int error = 0;
    int * perror = (int *)malloc(sizeof(int));
    bool eos = false;
    char *nalu_parse_buffer = NULL;
    uint32_t i;
    NvElementProfiler::NvElementProfilerData enc_data;
    NvElementProfiler::NvElementProfilerData dec_data;

    /* Set thread name for encoder Output Plane thread. */
    pthread_setname_np(pthread_self(),"DecOutPlane");

    ctx.dec = NvVideoDecoder::createVideoDecoder("dec0");

    ctx.enc = NvVideoEncoder::createVideoEncoder("enc0");

    ctx.in_file = new ifstream(ctx.in_file_path);
    TEST_ERROR(!ctx.in_file->is_open(), "Error opening input file", cleanup);

    ctx.out_file = new ofstream(ctx.out_file_path);
    TEST_ERROR(!ctx.out_file->is_open(), "Error opening output file", cleanup);

    ret = ctx.dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE, 0, 0);
    TEST_ERROR(ret < 0, "Could not subscribe to V4L2_EVENT_RESOLUTION_CHANGE",
               cleanup);

    if (ctx.use_gold_crc)
    {
        /* CRC specific initializetion if gold_crc flag is set */
        ctx.pBitStreamCrc = InitCrc(CRC32_POLYNOMIAL);
        TEST_ERROR(!ctx.pBitStreamCrc, "InitCrc failed", cleanup);
    }

    if (ctx.stats)
    {
        ctx.dec->enableProfiling();
        ctx.enc->enableProfiling();
    }

    ret = ctx.dec->setOutputPlaneFormat(ctx.decoder_pixfmt, CHUNK_SIZE);
    TEST_ERROR(ret < 0, "Could not set output plane format", cleanup);

    /* Configure for frame input mode for decoder.
       Refer V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT */
    if (ctx.input_nalu)
    {
         /* Input to the decoder will be nal units. */
         nalu_parse_buffer = new char[CHUNK_SIZE];
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

     /* Enable decoder error and metadata reporting.     Refer
        V4L2_CID_MPEG_VIDEO_ERROR_REPORTING */
    if (ctx.dec_enable_metadata || ctx.dec_input_metadata)
    {
        ret = ctx.dec->enableMetadataReporting();
        TEST_ERROR(ret < 0, "Error while enabling metadata reporting", cleanup);
    }

    /*  Enable max performance mode by using decoder max clock settings.
        Refer V4L2_CID_MPEG_VIDEO_MAX_PERFORMANCE */
    if (ctx.max_perf)
    {
        ret = ctx.dec->setMaxPerfMode(ctx.max_perf);
        TEST_ERROR(ret < 0, "Error while setting decoder to max perf", cleanup);
    }

    /* Query, Export and Map the output plane buffers so can read
       encoded data into the buffers. */
    if (ctx.dec_output_plane_mem_type == V4L2_MEMORY_MMAP)
    {
        /* configure decoder output plane for MMAP io-mode.
           Refer ioctl VIDIOC_REQBUFS, VIDIOC_QUERYBUF and VIDIOC_EXPBUF */
        ret = ctx.dec->output_plane.setupPlane(V4L2_MEMORY_MMAP, 2, true, false);
    }
    else if (ctx.dec_output_plane_mem_type == V4L2_MEMORY_USERPTR)
    {
        /* configure decoder output plane for USERPTR io-mode.
           Refer ioctl VIDIOC_REQBUFS */
        ret = ctx.dec->output_plane.setupPlane(V4L2_MEMORY_USERPTR, 10, false, true);
    }
    TEST_ERROR(ret < 0, "Error while setting up output plane", cleanup);

    ret = ctx.dec->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in output plane stream on", cleanup);

    if (ctx.copy_timestamp && ctx.input_nalu) {
      ctx.timestamp = (ctx.start_ts * MICROSECOND_UNIT);
      ctx.timestampincr = (MICROSECOND_UNIT * 16) / ((uint32_t) (ctx.dec_fps * 16));
    }

    if (ctx.stats)
    {
        ctx.enc->enableProfiling();
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
                (ctx.decoder_pixfmt == V4L2_PIX_FMT_H265))
        {
            if (ctx.input_nalu)
            {
                /* read the input nal unit. */
                read_decoder_input_nalu(&ctx, buffer, nalu_parse_buffer,
                        CHUNK_SIZE);
            }
            else
            {
                /* read the input chunks. */
                read_decoder_input_chunk(&ctx, buffer);
            }
        }

        if (ctx.decoder_pixfmt == V4L2_PIX_FMT_VP9 || ctx.decoder_pixfmt == V4L2_PIX_FMT_VP8)
        {
            /* read the input chunks. */
            ret = read_vpx_decoder_input_chunk(&ctx, buffer);
            if (ret != 0)
            {
                cerr << "Couldn't read chunk" << endl;
            }
        }

        if (ctx.input_nalu && ctx.copy_timestamp && ctx.flag_copyts)
        {
            /* Update the timestamp. */
            v4l2_buf.flags |= V4L2_BUF_FLAG_TIMESTAMP_COPY;
            ctx.timestamp += ctx.timestampincr;
            v4l2_buf.timestamp.tv_sec = ctx.timestamp / (MICROSECOND_UNIT);
            v4l2_buf.timestamp.tv_usec = ctx.timestamp % (MICROSECOND_UNIT);
        }

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;

        if (stream_stats[ctx.thread_num]->start_time.tv_sec == 0)
        {
            GET_TIME(&stream_stats[ctx.thread_num]->start_time);
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

    pthread_create(&ctx.dec_capture_loop, NULL, dec_capture_loop_fcn, &ctx);
        /* Set thread name for decoder Capture Plane thread. */
    pthread_setname_np(ctx.dec_capture_loop, "DecCapPlane");

    eos = transcoder_proc_blocking(ctx, eos, nalu_parse_buffer);

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

        if ((v4l2_buf.flags & V4L2_BUF_FLAG_ERROR) && ctx.dec_input_metadata)
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
                  abort(&ctx);
                  break;
                }
            }
        }
    }

    /* Signal EOS to the decoder capture loop. */
    ctx.got_eos = true;

cleanup:

    if (ctx.blocking_mode && ctx.dec_capture_loop)
    {
        pthread_join(ctx.dec_capture_loop, NULL);
        ctx.enc->capture_plane.waitForDQThread(-1);
    }

    if (ctx.stats)
    {
        if (stream_stats[ctx.thread_num]->end_time.tv_sec == 0)
        {
            GET_TIME(&stream_stats[ctx.thread_num]->end_time);
        }

        cout << "Stats for instance " << ctx.thread_num << endl;
        ctx.dec->getProfilingData(dec_data);
        ctx.enc->getProfilingData(enc_data);

        ctx.dec->printProfilingStats(cout);
        ctx.enc->printProfilingStats(cout);
        stream_stats[ctx.thread_num]->filename = strdup(ctx.in_file_path);
        stream_stats[ctx.thread_num]->enc_data = enc_data;
        stream_stats[ctx.thread_num]->dec_data = dec_data;
        stream_stats[ctx.thread_num]->thread_num = ctx.thread_num;
    }

    if (ctx.enc && ctx.enc->isInError())
    {
        cerr << "Encoder is in error" << endl;
        error = 1;
    }
    if (ctx.got_error)
    {
        error = 1;
    }

    if (ctx.dec_capture_plane_mem_type == V4L2_MEMORY_DMABUF)
    {
        for (int index = 0 ; index < ctx.num_cap_buffers ; index++)
        {
            if (ctx.dmabuff_fd[index] != 0)
            {
                ret = NvBufferDestroy (ctx.dmabuff_fd[index]);
                if (ret < 0)
                {
                    cerr << "Failed to Destroy NvBuffer" << endl;
                }
            }
        }
    }

    if (ctx.pBitStreamCrc)
    {
        char *pgold_crc = ctx.gold_crc;
        Crc *pout_crc= ctx.pBitStreamCrc;
        char StrCrcValue[20];
        snprintf (StrCrcValue, 20, "%u", pout_crc->CrcValue);
        /* Remove CRLF from end of CRC, if present */
        do {
                uint32_t len = strlen(pgold_crc);
                if (len == 0)
                {
                    break;
                }
                if (pgold_crc[len-1] == '\n')
                {
                    pgold_crc[len-1] = '\0';
                }
                else if (pgold_crc[len-1] == '\r')
                {
                    pgold_crc[len-1] = '\0';
                }
                else
                {
                    break;
                }
        } while(1);

        /* Check with golden CRC */
        if (strcmp (StrCrcValue, pgold_crc))
        {
            cout << "======================" << endl;
            cout << "video_encode: CRC FAILED" << endl;
            cout << "======================" << endl;
            cout << "Encoded CRC: " << StrCrcValue << " Gold CRC: " << pgold_crc << endl;
            error = 1;
        }
        else
        {
            cout << "======================" << endl;
            cout << "video_encode: CRC PASSED" << endl;
            cout << "======================" << endl;
        }

        CloseCrc(&ctx.pBitStreamCrc);
    }

    ctx.dec->output_plane.deinitPlane();

    ctx.dec->capture_plane.deinitPlane();

    ctx.enc->output_plane.deinitPlane();

    ctx.enc->capture_plane.deinitPlane();

    /* Release encoder configuration specific resources. */
    delete ctx.enc;
    delete ctx.dec;
    delete ctx.in_file;
    delete ctx.out_file;
    delete ctx.recon_Ref_file;
    delete[] nalu_parse_buffer;

    free(ctx.in_file_path);
    free(ctx.out_file_path);
    delete ctx.runtime_params_str;

    if (!ctx.blocking_mode)
    {
        sem_destroy(&ctx.pollthread_sema);
        sem_destroy(&ctx.encoderthread_sema);
    }

    if (-error == 0)
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
  * Start of video Transcode application.
  *
  * @param argc : Argument Count
  * @param argv : Argument Vector
  */
int
main(int argc, char *argv[])
{
    context_t **ctx;
    int num_files;
    int iterations;
    int stats;
    /* save decode iterator number */
    int iterator_num = 0;
    void * error;
    int ret = 0;

    num_files = get_num_files(argc, argv);

    if (num_files == -1)
    {
        fprintf(stderr, "Error parsing commandline arguments\n");
        return -1;
    }

    ctx = (context_t **)malloc(num_files * sizeof(context_t *));
    stream_stats = (fps_stats **)malloc(num_files * sizeof(fps_stats *));

    argv+=2;

    do
    {
        /* set defaults */
        set_defaults (ctx,stream_stats,num_files);

        /* parse the arguments */
        if (parse_csv_args(ctx, argc-3, argv, num_files))
        {
            fprintf(stderr, "Error parsing commandline arguments\n");
            return -1;
        }

        iterations = ctx[0]->num_iterations;
        stats = ctx[0]->stats;
        for (int i = 0 ; i < num_files ; i++)
        {
            /* Spawn multiple decoding threads for multiple decoders. */
            pthread_create(&(ctx[i]->transcode_thread), NULL, transcode_proc, ctx[i]);
            char dec_output_plane[16] = "DecOutplane";
            string s = to_string(i);
            strcat(dec_output_plane, s.c_str());
            /* Name each spawned thread. */
            pthread_setname_np(ctx[i]->transcode_thread, dec_output_plane);
        }

        for (int i = 0 ; i < num_files ; i++)
        {
            /* Wait for the decoding thread */
            pthread_join(ctx[i]->transcode_thread, &error);
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
            print_stats( num_files);
            for (int i = 0 ; i < num_files ; i++)
            {
                free (stream_stats[i]->filename);
                free (stream_stats[i]);
            }
        }
    } while(!ctx[0]->seek_mode && iterator_num < iterations);

    free (ctx);

    /* Report application run status on exit. */
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