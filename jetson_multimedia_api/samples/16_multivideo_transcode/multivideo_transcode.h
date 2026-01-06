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
#include "NvVideoEncoder.h"
#include "NvVideoDecoder.h"
#include <unistd.h>
#include <sstream>
#include <stdint.h>
#include <semaphore.h>

#define CRC32_POLYNOMIAL  0xEDB88320L
#define MAX_BUFFERS 32
#define NUM_ENCODER_OUTPUT_BUFFERS 6
#define CHUNK_SIZE 4000000

#define IVF_FILE_HDR_SIZE   32
#define IVF_FRAME_HDR_SIZE  12
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

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

#define IS_H264_NAL_CODED_SLICE(buffer_ptr) ((buffer_ptr[0] & 0x1F) == H264_NAL_UNIT_CODED_SLICE)
#define IS_H264_NAL_CODED_SLICE_IDR(buffer_ptr) ((buffer_ptr[0] & 0x1F) == H264_NAL_UNIT_CODED_SLICE_IDR)

#define GET_H265_NAL_UNIT_TYPE(buffer_ptr) ((buffer_ptr[0] & 0x7E) >> 1)

#define GET_TIME(timeval) clock_gettime(CLOCK_MONOTONIC,timeval);

#define TIMESPEC_DIFF_USEC(timespec1, timespec2) \
    (((timespec1)->tv_sec - (timespec2)->tv_sec) * 1000000000L + \
        (timespec1)->tv_nsec - (timespec2)->tv_nsec)

#define TEST_ERROR(cond, str, label) if (cond) { \
                                        cerr << str << endl; \
                                        error = 1; \
                                        goto label; }

#define TEST_PARSE_ERROR(cond, label) if (cond) { \
    cerr << "Error parsing runtime parameter changes string" << endl; \
    goto label; }

#define IS_DIGIT(c) (c >= '0' && c <= '9')
#define MICROSECOND_UNIT 1000000

typedef struct CrcRec
{
    uint32_t CRCTable[256];
    uint32_t CrcValue;
}Crc;

typedef struct
{
    NvVideoEncoder *enc;
    NvVideoDecoder *dec;
    uint32_t encoder_pixfmt;
    uint32_t raw_pixfmt;
    uint32_t decoder_pixfmt;
    char *in_file_path;
    std::ifstream *in_file;
    uint32_t width;
    uint32_t height;
    char *out_file_path;
    std::ofstream *out_file;
    std::ifstream *recon_Ref_file;
    uint32_t bitrate;
    uint32_t peak_bitrate;
    uint32_t profile;
    enum v4l2_mpeg_video_bitrate_mode ratecontrol;
    uint32_t iframe_interval;
    uint32_t idr_interval;
    uint32_t level;
    uint32_t fps_n;
    uint32_t fps_d;
    enum v4l2_enc_temporal_tradeoff_level_type temporal_tradeoff_level;
    enum v4l2_enc_hw_preset_type hw_preset_type;
    v4l2_enc_slice_length_type slice_length_type;
    uint32_t slice_length;
    uint32_t virtual_buffer_size;
    uint32_t num_reference_frames;
    uint32_t slice_intrarefresh_interval;
    uint32_t num_b_frames;
    uint32_t nMinQpI;              /* Minimum QP value to use for index frames */
    uint32_t nMaxQpI;              /* Maximum QP value to use for index frames */
    uint32_t nMinQpP;              /* Minimum QP value to use for P frames */
    uint32_t nMaxQpP;              /* Maximum QP value to use for P frames */
    uint32_t nMinQpB;              /* Minimum QP value to use for B frames */
    uint32_t nMaxQpB;              /* Maximum QP value to use for B frames */
    int output_plane_fd[32];
    bool insert_sps_pps_at_idr;
    bool disable_cabac;
    bool insert_vui;
    bool enable_extended_colorformat;
    bool insert_aud;
    bool alliframes;
    bool disable_dpb;
    bool dec_enable_metadata;
    bool dec_input_metadata;
    bool enc_report_metadata;
    enum v4l2_memory enc_output_memory_type;
    enum v4l2_memory enc_capture_memory_type;
    enum v4l2_memory dec_output_plane_mem_type;
    enum v4l2_memory dec_capture_plane_mem_type;
    enum v4l2_colorspace cs;
    bool input_nalu;
    bool copy_timestamp;
    bool flag_copyts;
    float dec_fps;
    uint32_t start_ts;
    bool dump_mv;
    bool bnoIframe;
    uint32_t nH264FrameNumBits;
    uint32_t nH265PocLsbBits;
    uint32_t dec_vp8_file_header_flag;
    uint32_t dec_vp9_file_header_flag;
    bool b_use_enc_cmd;
    bool enable_lossless;
    bool got_eos;
    bool use_gold_crc;
    char gold_crc[20];
    Crc *pBitStreamCrc;
    uint64_t timestamp;
    uint64_t timestampincr;
    bool stats;
    std::stringstream *runtime_params_str;
    uint32_t next_param_change_frame;
    bool got_error;
    bool seek_mode;
    int iterator_num;
    int num_iterations;
    uint32_t endofstream_capture;
    uint32_t endofstream_output;
    uint32_t stop_refill;
    uint32_t eos_recieved;
    uint32_t input_frames_queued_count;
    uint32_t num_output_buffers;
    int32_t num_frames_to_encode;
    uint32_t poc_type;
    uint32_t thread_num;
    int max_perf;
    int extra_cap_plane_buffer;
    int dmabuff_fd[MAX_BUFFERS];
    int num_cap_buffers;
    int blocking_mode; //Set if running in blocking mode
    sem_t pollthread_sema; // Polling thread waits on this to be signalled to issue Poll
    sem_t encoderthread_sema; // Encoder thread waits on this to be signalled to continue q/dq loop
    pthread_t enc_pollthread; // Polling thread, created if running in non-blocking mode.
    pthread_t enc_capture_loop; // Encoder capture thread
    pthread_t dec_capture_loop;
    pthread_t transcode_thread;
    pthread_t buffer_refill;
} context_t;

typedef struct
{
    char *filename;
    NvElementProfiler::NvElementProfilerData enc_data;
    NvElementProfiler::NvElementProfilerData dec_data;
    uint32_t thread_num;
    struct timespec start_time;
    struct timespec end_time;
} fps_stats;

int parse_csv_args(context_t ** ctx, int argc, char *argv[], int num_files);
int get_num_files(int argc, char *argv[]);
