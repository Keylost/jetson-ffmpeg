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

#include <cstdint>
#include <fstream>
#include "NvVideoEncoder.h"
#include <semaphore.h>
#include <stdint.h>
#include <string>

using namespace std;

typedef struct
{
    NvVideoEncoder *enc;
    uint32_t encoder_pixfmt;
    uint32_t raw_pixfmt;
    uint32_t thread_num;

    string in_file_path;
    string out_file_path;
    std::ifstream *in_file;
    std::ofstream *out_file;
    std::ofstream *mv_dump_file;

    uint32_t width;
    uint32_t height;

    uint32_t profile;
    uint32_t bitrate;
    uint32_t peak_bitrate;
    uint32_t level;
    uint32_t iframe_interval;
    uint32_t idr_interval;
    uint32_t fps_n;
    uint32_t fps_d;
    enum v4l2_enc_hw_preset_type hw_preset_type;
    uint32_t num_reference_frames;
    enum v4l2_mpeg_video_bitrate_mode ratecontrol;
    enum v4l2_memory output_memory_type;
    int output_plane_fd[32];
    bool insert_vui;
    bool enable_extended_colorformat;
    bool dump_mv;
    bool enableLossless;
    bool got_error;
    bool got_eos;
    int stress_test;

    uint32_t num_output_buffers;
    uint32_t input_frames_queued_count;

    bool blocking_mode; // True(default) if running in blocking mode
    sem_t pollthread_sema; // Polling thread waits on this to be signalled to issue Poll
    sem_t encoderthread_sema; // Encoder thread waits on this to be signalled to continue q/dq loop
    pthread_t enc_pollthread; // Polling thread, created if running in non-blocking mode.
    pthread_t encode_thread; // Current thread, encoding stream
} context_t;

int get_num_files (int argc, char *argv[]);
int parse_csv_args (context_t ** ctx, int argc, char *argv[], int num_files);