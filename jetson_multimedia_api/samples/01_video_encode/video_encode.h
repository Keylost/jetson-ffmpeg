/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION. All rights reserved.
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
#include <sstream>
#include <stdint.h>
#include <semaphore.h>

#define CRC32_POLYNOMIAL  0xEDB88320L
#define MAX_OUT_BUFFERS 32

typedef struct RPS_List
{
    uint32_t nFrameId;
    bool bLTRefFrame;
} RPS_List;

typedef struct RPS_param
{
    sem_t sema;
    uint32_t m_numTemperalLayers;
    uint32_t nActiveRefFrames;
    RPS_List rps_list[V4L2_MAX_REF_FRAMES];
} RPS_param;

typedef struct CrcRec
{
    unsigned int CRCTable[256];
    unsigned int CrcValue;
}Crc;

typedef struct
{
    NvVideoEncoder *enc;
    uint32_t encoder_pixfmt;
    uint32_t raw_pixfmt;

    char *in_file_path;
    std::ifstream *in_file;

    uint32_t width;
    uint32_t height;

    char *out_file_path;
    std::ofstream *out_file;

    char *ROI_Param_file_path;
    char *Recon_Ref_file_path;
    char *RPS_Param_file_path;
    char *hints_Param_file_path;
    char *GDR_Param_file_path;
    char *GDR_out_file_path;
    std::ifstream *roi_Param_file;
    std::ifstream *recon_Ref_file;
    std::ifstream *rps_Param_file;
    std::ifstream *hints_Param_file;
    std::ifstream *gdr_Param_file;
    std::ofstream *gdr_out_file;

    uint32_t bitrate;
    uint32_t peak_bitrate;
    uint32_t profile;
    enum v4l2_mpeg_video_bitrate_mode ratecontrol;
    uint32_t iframe_interval;
    uint32_t idr_interval;
    uint32_t level;
    uint32_t fps_n;
    uint32_t fps_d;
    uint32_t gdr_start_frame_number; /* Frame number where GDR has to be started */
    uint32_t gdr_num_frames; /* Number of frames where GDR to be applied */
    uint32_t gdr_out_frame_number; /* Frames number from where encoded buffers are to be dumped */
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
    uint32_t sMaxQp;               /* Session Maximum QP value */
    uint32_t sar_width;
    uint32_t sar_height;
    uint8_t bit_depth;
    uint8_t chroma_format_idc;
    int output_plane_fd[32];
    bool insert_sps_pps_at_idr;
    bool enable_slice_level_encode;
    bool disable_cabac;
    bool insert_vui;
    bool enable_extended_colorformat;
    bool insert_aud;
    bool alliframes;
    bool is_semiplanar;
    bool disable_amp;
    enum v4l2_memory output_memory_type;
    enum v4l2_colorspace cs;

    bool report_metadata;
    bool input_metadata;
    bool copy_timestamp;
    uint32_t start_ts;
    bool dump_mv;
    bool enableGDR;
    bool bGapsInFrameNumAllowed;
    bool bnoIframe;
    uint32_t nH264FrameNumBits;
    uint32_t nH265PocLsbBits;
    bool externalRCHints;
    bool enableROI;
    bool b_use_enc_cmd;
    bool enableLossless;
    bool got_eos;

    bool externalRPS;
    bool RPS_threeLayerSvc;
    RPS_param rps_par;

    bool use_gold_crc;
    char gold_crc[20];
    Crc *pBitStreamCrc;

    bool bReconCrc;
    uint32_t rl;                   /* Reconstructed surface Left cordinate */
    uint32_t rt;                   /* Reconstructed surface Top cordinate */
    uint32_t rw;                   /* Reconstructed surface width */
    uint32_t rh;                   /* Reconstructed surface height */

    uint64_t timestamp;
    uint64_t timestampincr;

    bool stats;

    std::stringstream *runtime_params_str;
    uint32_t next_param_change_frame;
    bool got_error;
    int  stress_test;
    uint32_t endofstream_capture;
    uint32_t endofstream_output;

    uint32_t input_frames_queued_count;
    uint32_t startf;
    uint32_t endf;
    uint32_t num_output_buffers;
    int32_t num_frames_to_encode;
    uint32_t poc_type;

    int max_perf;
    int blocking_mode; //Set if running in blocking mode
    sem_t pollthread_sema; // Polling thread waits on this to be signalled to issue Poll
    sem_t encoderthread_sema; // Encoder thread waits on this to be signalled to continue q/dq loop
    pthread_t   enc_pollthread; // Polling thread, created if running in non-blocking mode.
    pthread_t enc_capture_loop; // Encoder capture thread
} context_t;

int parse_csv_args(context_t * ctx, int argc, char *argv[]);
