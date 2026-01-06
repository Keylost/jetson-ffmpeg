/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION. All rights reserved.
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

#include "NvVideoDecoder.h"
#include "NvVideoConverter.h"
#include "NvEglRenderer.h"
#include <queue>
#include <fstream>
#include <pthread.h>
#include <semaphore.h>

#define USE_NVBUF_TRANSFORM_API

#define MAX_BUFFERS 32

typedef struct
{
    NvVideoDecoder *dec;
    NvVideoConverter *conv;
    uint32_t decoder_pixfmt;

    NvEglRenderer *renderer;

    char *in_file_path;
    std::ifstream *in_file;

    char *out_file_path;
    std::ofstream *out_file;

    bool disable_rendering;
    bool fullscreen;
    uint32_t window_height;
    uint32_t thread_num;
    uint32_t window_width;
    uint32_t window_x;
    uint32_t window_y;
    uint32_t out_pixfmt;
    uint32_t video_height;
    uint32_t video_width;
    uint32_t display_height;
    uint32_t display_width;
    uint32_t file_count;
    float fps;

    bool disable_dpb;

    bool input_nalu;

    bool copy_timestamp;
    bool flag_copyts;
    uint32_t start_ts;
    float dec_fps;
    uint64_t timestamp;
    uint64_t timestampincr;

    bool stats;

    int  stress_test;
    bool enable_metadata;
    bool enable_input_metadata;
    enum v4l2_skip_frames_type skip_frames;
    enum v4l2_memory output_plane_mem_type;
    enum v4l2_memory capture_plane_mem_type;
#ifndef USE_NVBUF_TRANSFORM_API
    enum v4l2_yuv_rescale_method rescale_method;
#endif

    std::queue < NvBuffer * > *conv_output_plane_buf_queue;
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;

    sem_t pollthread_sema; // Polling thread waits on this to be signalled to issue Poll
    sem_t decoderthread_sema; // Decoder thread waits on this to be signalled to continue q/dq loop
    pthread_t dec_pollthread; // Polling thread, created if running in non-blocking mode.
    pthread_t decode_thread; // Current thread, decoding stream.
    pthread_t dec_capture_loop; // Decoder capture thread, created if running in blocking mode.
    bool got_error;
    bool got_eos;
    bool vp9_file_header_flag;
    bool vp8_file_header_flag;
    int dst_dma_fd;
    int dmabuff_fd[MAX_BUFFERS];
    int numCapBuffers;
    int loop_count;
    int blocking_mode; // Set to true if running in blocking mode
} context_t;

typedef struct
{
    char *filename;
    NvElementProfiler::NvElementProfilerData data;
    uint32_t thread_num;
} fps_stats;

int parse_csv_args(context_t ** ctx, int argc, char *argv[], int num_files);
int get_num_files(int argc, char *argv[]);
