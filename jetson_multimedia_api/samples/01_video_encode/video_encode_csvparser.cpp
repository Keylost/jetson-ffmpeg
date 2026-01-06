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

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <linux/v4l2-controls.h>
#include "video_encode.h"

#define CHECK_OPTION_VALUE(argp) if(!*argp || (*argp)[0] == '-') \
                                { \
                                    cerr << "Error: value not specified for option " << arg << endl; \
                                    goto error; \
                                }

#define CSV_PARSE_CHECK_ERROR(condition, str) \
    if (condition) {\
    cerr << "Error: " << str << endl; \
    goto error; \
    }

using namespace std;

static void
print_help(void)
{
    cerr << "\nvideo_encode <in-file> <in-width> <in-height> <encoder-type> <out-file> [OPTIONS]\n\n"
            "Encoder Types:\n"
            "\tH264\n"
            "\tH265\n"
            "\tVP8\n"
            "\tVP9\n\n"
            "OPTIONS:\n"
            "\t-h,--help             Prints this text\n"
            "\t--dbg-level <level>   Sets the debug level [Values 0-3]\n\n"
            "\t--stats               Report profiling data for the app\n\n"
            "\t-br <bitrate>         Bitrate [Default = 4000000]\n"
            "\t-pbr <peak_bitrate>   Peak bitrate [Default = 1.2*bitrate]\n\n"
            "NOTE: Peak bitrate takes effect in VBR more; must be >= bitrate\n\n"
            "\t-p <profile>          Encoding Profile [Default = baseline]\n"
            "\t-l <level>            Encoding Level [Default set by the library]\n"
            "\t-rc <rate-control>    Ratecontrol mode [Default = cbr]\n"
            "\t--elossless           Enable Lossless encoding [Default = disabled,\n"
            "\t                      Option applicable only with YUV444 input]\n"
            "\t-cf <val>             Set chroma factor IDC for H265. [Default = 1, set by encoder]\n"
            "\t-bd <bit-depth>       Bit-depth (Supported values are 8 and 10) [Default 8]\n"
            "\t                      Note: For 10-bit, YUV must be MSB aligned\n"
            "\t--sp                  Encode frames from semiplanar yuv input. [Default = disabled]\n"
            "\t                      Right now, application only supports NV12, NV24, NV24_10LE and P010_10LE\n"
            "\t--max-perf            Enable maximum Performance \n"
            "\t-ifi <interval>       I-frame Interval [Default = 30]\n"
            "\t-idri <interval>      IDR Interval [Default = 256]\n"
            "\t--insert-spspps-idr   Insert SPS PPS at every IDR [Default = disabled]\n"
            "\t--insert-vui          Insert VUI [Default = disabled]\n"
            "\t--enable-extcolorfmt  Set Extended ColorFormat (Only works with insert-vui) [Default = disabled]\n"
            "\t--color-space <num>   Specify colorspace [1 = BT.601(Default), 2 = BT.709]\n\n"
            "\t--insert-aud          Insert AUD [Default = disabled]\n"
            "\t--alliframes          Enable all I-frame encoding [Default = disabled]\n"
            "\t-fps <num> <den>      Encoding fps in num/den [Default = 30/1]\n\n"
            "\t-tt <level>           Temporal Tradeoff level [Default = 0]\n"
            "\t-vbs <size>           Virtual buffer size [Default = 0]\n"
            "\t-nrf <num>            Number of reference frames [Default = 1]\n\n"
            "\t-slt <type>           Slice length type (1 = Number of MBs, 2 = Bytes) [Default = 1]\n"
            "\t-hpt <type>           HW preset type (1 = ultrafast, 2 = fast, 3 = medium,  4 = slow)\n"
            "\t-slen <length>        Slice length [Default = 0]\n"
            "\t--sle                 Slice level encode output [Default = disabled]\n"
            "\t--cd                  CABAC Disable for H264 [Default = disabled]\n"
            "\t-sir <interval>       Slice intrarefresh interval [Default = 0]\n\n"
            "\t-nbf <num>            Number of B frames [Default = 0]\n\n"
            "\t-rpc <string>         Change configurable parameters at runtime\n\n"
            "\t-goldcrc <string>     GOLD CRC\n\n"
            "\t--rcrc                Reconstructed surface CRC\n\n"
            "\t-rl <cordinate>       Reconstructed surface Left cordinate [Default = 0]\n\n"
            "\t-rt <cordinate>       Reconstructed surface Top cordinate [Default = 0]\n\n"
            "\t-rw <val>             Reconstructed surface width\n\n"
            "\t-rh <val>             Reconstructed surface height\n\n"
            "\t-sar <width> <height> Sample Aspect Ratio parameters [Default = 0 0]\n\n"
            "\t-rcrcf <reconref_file_path> Specify recon crc reference param file\n\n"
            "\t--report-metadata     Print encoder output metadata\n"
            "\t--blocking-mode <val> Set blocking mode, 0 is non-blocking, 1 for blocking (Default) \n\n"
            "\t--input-metadata      Enable encoder input metadata\n"
            "\t--copy-timestamp <st> Enable copy timestamp with start timestamp(st) in seconds\n"
            "\t--mvdump              Dump encoded motion vectors\n\n"
            "\t--eroi                Enable ROI [Default = disabled]\n\n"
            "\t-roi <roi_file_path>  Specify roi param file\n\n"
            "\t--erps                Enable External RPS [Default = disabled]\n\n"
            "\t--svc                 Enable RPS Three Layer SVC [Default = disabled]\n\n"
            "\t--egdr                Enable GDR [Default = disabled]\n\n"
            "\t--gif                 Enable Gaps in FrameNum [Default = disabled]\n\n"
            "\t-fnb <num_bits>       H264 FrameNum bits [Default = 0]\n\n"
            "\t-plb <num_bits>       H265 poc lsb bits [Default = 0]\n\n"
            "\t--ni                  No I-frames [Default = disabled]\n\n"
            "\t-rpsf <rps_file_path> Specify external rps param file\n\n"
            "\t--erh                 Enable External picture RC [Default = disabled]\n\n"
            "\t-poc <type>           Specify POC type [Default = 0]\n\n"
            "\t-mem_type_oplane <num> Specify memory type for the output plane to be used [1 = V4L2_MEMORY_MMAP, 2 = V4L2_MEMORY_USERPTR, 3 = V4L2_MEMORY_DMABUF]\n\n"
            "\t-gdrf <gdr_file_path> Specify GDR Parameters filename \n\n"
            "\t-gdrof <gdr_out_file_path> Specify GDR Out filename \n\n"
            "\t-smq <max_qp_value>   Max QP per session when external picture RC enabled\n\n"
            "\t-hf <hint_file_path>  Specify external rate control param file\n\n"
            "\t-MinQpI               Specify minimum Qp Value for I frame\n\n"
            "\t-MaxQpI               Specify maximum Qp Value for I frame\n\n"
            "\t-MinQpP               Specify minimum Qp Value for P frame\n\n"
            "\t-MaxQpP               Specify maximum Qp Value for P frame\n\n"
            "\t-MinQpB               Specify minimum Qp Value for B frame\n\n"
            "\t-MaxQpB               Specify maximum Qp Value for B frame\n\n"
            "\t-s <loop-count>       Stress test [Default = 1]\n\n"
            "\t-sf <val>             Starting frame number [Default = 0]\n\n"
            "\t-ef <val>             Ending frame number [Default = 0, all frame will be encoded]\n\n"
            "\t--no_amp              Disable Aysmmetric Motion Partition. Applicable only for H.265 [Default = 0]\n\n"
            "NOTE: roi parameters need to be feed per frame in following format\n"
            "      <no. of roi regions> <Qpdelta> <left> <top> <width> <height> ...\n"
            "      e.g. [Each line corresponds roi parameters for one frame] \n"
            "      2   -2   34  33  16  19  -3  68  68  16  16\n"
            "      1   -5   40  40  40  40\n"
            "      3   -4   34  34  16  16  -5  70  70  18  18  -3  100  100  34  34\n"
            "NOTE: YUV frames must be MSB aligned for 10-bit encoding\n"
            "Supported Encoding profiles for H.264:\n"
            "\tbaseline\tmain\thigh\thigh444\n"
            "Supported Encoding profiles for H.265:\n"
            "\tmain\n"
            "\tmain10\n\n"
            "Supported Encoding levels for H.264\n"
            "\t1.0\t1b\t1.1\t1.2\t1.3\n"
            "\t2.0\t2.1\t2.2\n"
            "\t3.0\t3.1\t3.2\n"
            "\t4.0\t4.1\t4.2\n"
            "\t5.0\t5.1\n"
            "Supported Encoding levels for H.265\n"
            "\tmain1.0\thigh1.0\n"
            "\tmain2.0\thigh2.0\tmain2.1\thigh2.1\n"
            "\tmain3.0\thigh3.0\tmain3.1\thigh3.1\n"
            "\tmain4.0\thigh4.0\tmain4.1\thigh4.1\n"
            "\tmain5.0\thigh5.0\tmain5.1\thigh5.1\tmain5.2\thigh5.2\n"
            "\tmain6.0\thigh6.0\tmain6.1\thigh6.1\tmain6.2\thigh6.2\n\n"
            "Supported Encoding rate control modes:\n"
            "\tcbr\tvbr\n\n"
            "Supported Temporal Tradeoff levels:\n"
            "0:Drop None       1:Drop 1 in 5      2:Drop 1 in 3\n"
            "3:Drop 1 in 2     4:Drop 2 in 3\n\n"
            "Runtime configurable parameter string should be of the form:\n"
            "\"f<frame_num1>,<prop_id1><val>,<prop_id2><val>#f<frame_num1>,<prop_id1><val>,<prop_id2><val>#...\"\n"
            "e.g. \"f20,b8000000,i1#f300,b6000000,r40/1\"\n\n"
            "Property ids:\n"
            "\tb<bitrate>  Bitrate\n"
            "\tp<peak_bitrate>  Peak Bitrate\n"
            "\tr<num/den>  Framerate\n"
            "\ti1          Force I-frame\n\n"
            "NOTE: These encoding parameters are slightly imprecisely updated depending upon the number of\n"
            "frames in queue and/or processed already.\n";
}

static uint32_t
get_encoder_type(char *arg)
{
    if (!strcmp(arg, "H264"))
        return V4L2_PIX_FMT_H264;
    if (!strcmp(arg, "H265"))
        return V4L2_PIX_FMT_H265;
    if (!strcmp(arg, "VP8"))
        return V4L2_PIX_FMT_VP8;
    if (!strcmp(arg, "VP9"))
        return V4L2_PIX_FMT_VP9;
    return 0;
}

static int32_t
get_encoder_ratecontrol(char *arg)
{
    if (!strcmp(arg, "cbr"))
        return V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;

    if (!strcmp(arg, "vbr"))
        return V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;

    return -1;
}

static int32_t
get_encoder_profile_h264(char *arg)
{
    if (!strcmp(arg, "baseline"))
        return V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;

    if (!strcmp(arg, "main"))
        return V4L2_MPEG_VIDEO_H264_PROFILE_MAIN;

    if (!strcmp(arg, "high"))
        return V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;

    if (!strcmp(arg, "high444"))
        return V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_444_PREDICTIVE;
    return -1;
}

static int32_t
get_encoder_profile_h265(char *arg)
{
    if (!strcmp(arg, "main"))
        return V4L2_MPEG_VIDEO_H265_PROFILE_MAIN;

    if (!strcmp(arg, "main10"))
        return V4L2_MPEG_VIDEO_H265_PROFILE_MAIN10;

    return -1;
}

static int32_t
get_h264_encoder_level(char *arg)
{
    if (!strcmp(arg, "1.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_0;

    if (!strcmp(arg, "1b"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1B;

    if (!strcmp(arg, "1.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_1;

    if (!strcmp(arg, "1.2"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_2;

    if (!strcmp(arg, "1.3"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_3;

    if (!strcmp(arg, "2.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_2_0;

    if (!strcmp(arg, "2.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_2_1;

    if (!strcmp(arg, "2.2"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_2_2;

    if (!strcmp(arg, "3.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_3_0;

    if (!strcmp(arg, "3.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_3_1;

    if (!strcmp(arg, "3.2"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_3_2;

    if (!strcmp(arg, "4.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_4_0;

    if (!strcmp(arg, "4.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_4_1;

    if (!strcmp(arg, "4.2"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_4_2;

    if (!strcmp(arg, "5.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_5_0;

    if (!strcmp(arg, "5.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_5_1;

    return -1;
}

static int32_t
get_h265_encoder_level(char *arg)
{
    if (!strcmp(arg, "main1.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_1_0_MAIN_TIER;

    if (!strcmp(arg, "high1.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_1_0_HIGH_TIER;

    if (!strcmp(arg, "main2.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_2_0_MAIN_TIER;

    if (!strcmp(arg, "high2.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_2_0_HIGH_TIER;

    if (!strcmp(arg, "main2.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_2_1_MAIN_TIER;

    if (!strcmp(arg, "high2.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_2_1_HIGH_TIER;

    if (!strcmp(arg, "main3.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_3_0_MAIN_TIER;

    if (!strcmp(arg, "high3.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_3_0_HIGH_TIER;

    if (!strcmp(arg, "main3.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_3_1_MAIN_TIER;

    if (!strcmp(arg, "high3.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_3_1_HIGH_TIER;

    if (!strcmp(arg, "main4.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_4_0_MAIN_TIER;

    if (!strcmp(arg, "high4.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_4_0_HIGH_TIER;

    if (!strcmp(arg, "main4.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_4_1_MAIN_TIER;

    if (!strcmp(arg, "high4.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_4_1_HIGH_TIER;

    if (!strcmp(arg, "main5.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_0_MAIN_TIER;

    if (!strcmp(arg, "high5.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_0_HIGH_TIER;

    if (!strcmp(arg, "main5.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_1_MAIN_TIER;

    if (!strcmp(arg, "high5.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_1_HIGH_TIER;

    if (!strcmp(arg, "main5.2"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_2_MAIN_TIER;

    if (!strcmp(arg, "high5.2"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_2_HIGH_TIER;

    if (!strcmp(arg, "main6.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_0_MAIN_TIER;

    if (!strcmp(arg, "high6.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_0_HIGH_TIER;

    if (!strcmp(arg, "main6.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_1_MAIN_TIER;

    if (!strcmp(arg, "high6.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_1_HIGH_TIER;

    if (!strcmp(arg, "main6.2"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_2_MAIN_TIER;

    if (!strcmp(arg, "high6.2"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_2_HIGH_TIER;

    return -1;
}

static int32_t
get_dbg_level(char *arg)
{
    int32_t log_level = atoi(arg);

    if (log_level < 0)
    {
        cout << "Warning: invalid log level input, defaulting to setting 0" << endl;
        return 0;
    }

    if (log_level > 3)
    {
        cout << "Warning: invalid log level input, defaulting to setting 3" << endl;
        return 3;
    }

    return log_level;
}

int
parse_csv_args(context_t * ctx, int argc, char *argv[])
{
    char **argp = argv;
    char *arg = *(++argp);
    int32_t intval = -1;

    if (argc == 1 || (arg && (!strcmp(arg, "-h") || !strcmp(arg, "--help"))))
    {
        print_help();
        exit(EXIT_SUCCESS);
    }

    CSV_PARSE_CHECK_ERROR(argc < 6, "Insufficient arguments");

    ctx->in_file_path = strdup(*argp);
    CSV_PARSE_CHECK_ERROR(!ctx->in_file_path, "Input file not specified");

    ctx->width = atoi(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->width == 0, "Input width should be > 0");

    ctx->height = atoi(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->height == 0, "Input height should be > 0");

    ctx->encoder_pixfmt = get_encoder_type(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->encoder_pixfmt == 0,
                          "Incorrect encoder type");

    ctx->out_file_path = strdup(*(++argp));
    CSV_PARSE_CHECK_ERROR(!ctx->out_file_path, "Output file not specified");

    while ((arg = *(++argp)))
    {
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
            print_help();
            exit(EXIT_SUCCESS);
        }
        else if (!strcmp(arg, "--dbg-level"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            log_level = get_dbg_level(*argp);
        }
        else if (!strcmp(arg, "--stats"))
        {
            ctx->stats = true;
        }
        else if (!strcmp(arg, "--eroi"))
        {
            ctx->enableROI = true;
        }
        else if (!strcmp(arg, "-roi"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->ROI_Param_file_path = strdup(*argp);
        }
        else if (!strcmp(arg, "--erps"))
        {
            ctx->externalRPS = true;
        }
        else if (!strcmp(arg, "--svc"))
        {
            ctx->RPS_threeLayerSvc = true;
        }
        else if(!strcmp(arg, "--max-perf"))
        {
            ctx->max_perf = 1;
        }
        else if (!strcmp(arg, "--egdr"))
        {
            ctx->enableGDR = true;
        }
        else if (!strcmp(arg, "--gif"))
        {
            ctx->bGapsInFrameNumAllowed = true;
        }
        else if (!strcmp(arg, "-fnb"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->nH264FrameNumBits = atoi(*argp);
        }
        else if (!strcmp(arg, "-plb"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->nH265PocLsbBits = atoi(*argp);
        }
        else if (!strcmp(arg, "--ni"))
        {
            ctx->bnoIframe = true;
        }
        else if (!strcmp(arg, "-rpsf"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->RPS_Param_file_path = strdup(*argp);
        }
        else if (!strcmp(arg, "-gdrf"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->GDR_Param_file_path = strdup(*argp);
        }
        else if (!strcmp(arg, "-gdrof"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->GDR_out_file_path = strdup(*argp);
        }
        else if (!strcmp(arg, "--erh"))
        {
            ctx->externalRCHints = true;
        }
        else if (!strcmp(arg, "-smq"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->sMaxQp = (uint32_t) atoi(*argp);
//          TODO : Check for valid sMaxQp range
//          CSV_PARSE_CHECK_ERROR(ctx->sMaxQp == 0,
//                  "Session max QP value should be > 0");
        }
        else if (!strcmp(arg, "-hf"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->hints_Param_file_path = strdup(*argp);
        }
        else if (!strcmp(arg, "-fps"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->fps_n = atoi(*argp);
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->fps_d = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->fps_d == 0, "fps den should be > 0");
        }
        else if (!strcmp(arg, "-s"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->stress_test = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->stress_test <= 0,
                    "stress times should be bigger than 0");
        }
        else if (!strcmp(arg, "-br"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->bitrate = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->bitrate == 0, "bit rate should be > 0");
        }
        else if (!strcmp(arg, "-pbr"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->peak_bitrate = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->peak_bitrate == 0, "bit rate should be > 0");
        }
        else if (!strcmp(arg, "-ifi"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->iframe_interval = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->iframe_interval == 0,
                    "ifi size shoudl be > 0");
        }
        else if (!strcmp(arg, "-idri"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->idr_interval = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->idr_interval == 0,
                    "idri size shoudl be > 0");
        }
        else if (!strcmp(arg, "--insert-spspps-idr"))
        {
            ctx->insert_sps_pps_at_idr = true;
        }
        else if (!strcmp(arg, "--sle"))
        {
            ctx->enable_slice_level_encode = true;
            ctx->num_output_buffers = MAX_OUT_BUFFERS;
        }
        else if (!strcmp(arg, "--cd"))
        {
            ctx->disable_cabac = true;
        }
        else if (!strcmp(arg, "--insert-vui"))
        {
            ctx->insert_vui = true;
        }
        else if (!strcmp(arg, "--enable-extcolorfmt"))
        {
            ctx->enable_extended_colorformat = true;
        }
        else if (!strcmp(arg, "--insert-aud"))
        {
            ctx->insert_aud = true;
        }
        else if (!strcmp(arg, "--alliframes"))
        {
            ctx->alliframes = true;
        }
        else if (!strcmp(arg, "-l"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            if (ctx->encoder_pixfmt == V4L2_PIX_FMT_H264)
            {
                ctx->level = get_h264_encoder_level(*argp);
            }
            else if (ctx->encoder_pixfmt == V4L2_PIX_FMT_H265)
            {
                ctx->level = get_h265_encoder_level(*argp);
            }
            CSV_PARSE_CHECK_ERROR(ctx->level == (uint32_t)-1,
                    "Unsupported value for level: " << *argp);
        }
        else if (!strcmp(arg, "-rc"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            intval = get_encoder_ratecontrol(*argp);
            CSV_PARSE_CHECK_ERROR(intval == -1,
                    "Unsupported value for ratecontrol: " << *argp);
            ctx->ratecontrol = (enum v4l2_mpeg_video_bitrate_mode) intval;
        }
        else if (!strcmp(arg, "--elossless"))
        {
            ctx->enableLossless = true;
        }
        else if (!strcmp(arg, "--no_amp"))
        {
            ctx->disable_amp = true;
        }
        else if (!strcmp(arg, "-cf"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->chroma_format_idc = (uint8_t) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(((ctx->chroma_format_idc != 1) && (ctx->chroma_format_idc != 3)),
                    "Only 1 or 3 supported");
        }
        else if (!strcmp(arg, "-bd"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->bit_depth = (uint8_t) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(((ctx->bit_depth != 8) && (ctx->bit_depth != 10)),
                    "Only 8/10-Bit depth supported");
        }
        else if (!strcmp(arg, "--sp"))
        {
            ctx->is_semiplanar = true;
        }
        else if (!strcmp(arg, "-rpc"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->runtime_params_str = new stringstream(*argp);
        }
        else if (!strcmp(arg, "-goldcrc"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            snprintf(ctx->gold_crc,sizeof(ctx->gold_crc),"%s", *argp);
            ctx->use_gold_crc = true;
        }
        else if (!strcmp(arg, "-p"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            if (ctx->encoder_pixfmt == V4L2_PIX_FMT_H264)
            {
                ctx->profile = get_encoder_profile_h264(*argp);
            }
            else if (ctx->encoder_pixfmt == V4L2_PIX_FMT_H265)
            {
                ctx->profile = get_encoder_profile_h265(*argp);
            }
            CSV_PARSE_CHECK_ERROR(ctx->profile == (uint32_t) -1,
                        "Unsupported value for profile: " << *argp);
        }
        else if (!strcmp(arg, "-tt"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->temporal_tradeoff_level =
                (enum v4l2_enc_temporal_tradeoff_level_type) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->temporal_tradeoff_level <
                     V4L2_ENC_TEMPORAL_TRADEOFF_LEVEL_DROPNONE ||
                     ctx->temporal_tradeoff_level >
                     V4L2_ENC_TEMPORAL_TRADEOFF_LEVEL_DROP2IN3),
                    "Unsupported value for temporal tradeoff: " << *argp);
        }
        else if (!strcmp(arg, "-slt"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            switch (atoi(*argp))
            {
                case 1:
                    ctx->slice_length_type = V4L2_ENC_SLICE_LENGTH_TYPE_MBLK;
                    break;
                case 2:
                    ctx->slice_length_type = V4L2_ENC_SLICE_LENGTH_TYPE_BITS;
                    break;
                default:
                    CSV_PARSE_CHECK_ERROR(true,
                            "Unsupported value for slice length type: " << *argp);
            }
        }
        else if (!strcmp(arg, "-hpt"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            switch (atoi(*argp))
            {
                case 1:
                    ctx->hw_preset_type = V4L2_ENC_HW_PRESET_ULTRAFAST;
                    break;
                case 2:
                    ctx->hw_preset_type = V4L2_ENC_HW_PRESET_FAST;
                    break;
                case 3:
                    ctx->hw_preset_type = V4L2_ENC_HW_PRESET_MEDIUM;
                    break;
                case 4:
                    ctx->hw_preset_type = V4L2_ENC_HW_PRESET_SLOW;
                    break;
                default:
                    CSV_PARSE_CHECK_ERROR(true,
                            "Unsupported value for encoder HW Preset Type: " << *argp);
            }
        }
        else if (!strcmp(arg, "-slen"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->slice_length = (uint32_t) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->slice_length == 0,
                    "Slice length should be > 0");
        }
        else if (!strcmp(arg, "-vbs"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->virtual_buffer_size = (uint32_t) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->virtual_buffer_size == 0,
                    "Virtual buffer size should be > 0");
        }
        else if (!strcmp(arg, "-nbf"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->num_b_frames = (uint32_t) atoi(*argp);
        }
        else if (!strcmp(arg, "-nrf"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->num_reference_frames = (uint32_t) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->num_reference_frames == 0,
                    "Num reference frames should be > 0");
        }
        else if (!strcmp(arg, "--color-space"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            int num = (uint32_t) atoi(*argp);
            switch(num)
            {
                case 1  :ctx->cs = V4L2_COLORSPACE_SMPTE170M;
                         break;
                case 2  :ctx->cs = V4L2_COLORSPACE_REC709;
                         break;
            }
            CSV_PARSE_CHECK_ERROR(!(num>0&&num<3),
                    "Color space should be > 0 and < 4");
        }
        else if (!strcmp(arg, "-mem_type_oplane"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            int num = (uint32_t) atoi(*argp);
            switch(num)
            {
                case 1  :ctx->output_memory_type = V4L2_MEMORY_MMAP;
                         break;
                case 2  :ctx->output_memory_type = V4L2_MEMORY_USERPTR;
                         break;
                case 3  :ctx->output_memory_type = V4L2_MEMORY_DMABUF;
                         break;
            }
            CSV_PARSE_CHECK_ERROR(!(num>0&&num<4),
                    "Memory type selection should be > 0 and < 4");
        }
        else if (!strcmp(arg, "-sir"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->slice_intrarefresh_interval = (uint32_t) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->slice_intrarefresh_interval == 0,
                    "Slice intrarefresh interval should be > 0");
        }
        else if (!strcmp(arg, "-MinQpI"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->nMinQpI = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->nMinQpI > 51, "Min Qp should be >= 0 and <= 51");
        }
        else if (!strcmp(arg, "-MaxQpI"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->nMaxQpI = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->nMaxQpI > 51, "Max Qp should be >= 0 and <= 51");
        }
        else if (!strcmp(arg, "-MinQpP"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->nMinQpP = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->nMinQpP > 51, "Min Qp should be >= 0 and <= 51");
        }
        else if (!strcmp(arg, "-MaxQpP"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->nMaxQpP = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->nMaxQpP > 51, "Max Qp should be >= 0 and <= 51");
        }
        else if (!strcmp(arg, "-MinQpB"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->nMinQpB = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->nMinQpB > 51, "Min Qp should be >= 0 and <= 51");
        }
        else if (!strcmp(arg, "-MaxQpB"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->nMaxQpB = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->nMaxQpB > 51, "Max Qp should be >= 0 and <= 51");
        }
        else if (!strcmp(arg, "--rcrc"))
        {
            ctx->bReconCrc = true;
        }
        else if (!strcmp(arg, "-rl"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->rl = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->rl < 0, "Reconstructed surface Left cordinate should be >= 0");
        }
        else if (!strcmp(arg, "-rt"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->rt = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->rt < 0, "Reconstructed surface Top cordinate should be >= 0");
        }
        else if (!strcmp(arg, "-rw"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->rw = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->rw <= 0, "Reconstructed surface width should be > 0");
        }
        else if (!strcmp(arg, "-rh"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->rh = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->rh <= 0, "Reconstructed surface height should be > 0");
        }
        else if (!strcmp(arg, "-sar"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->sar_width = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->sar_width <= 0, "SAR width should be > 0");
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->sar_height = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->fps_d == 0, "SAR height should be > 0");
        }
        else if (!strcmp(arg, "-rcrcf"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->Recon_Ref_file_path = strdup(*argp);
        }
        else if (!strcmp(arg, "--report-metadata"))
        {
            ctx->report_metadata = true;
        }
        else if (!strcmp(arg, "--input-metadata"))
        {
            ctx->input_metadata = true;
        }
        else if (!strcmp(arg, "--copy-timestamp"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->start_ts = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->start_ts < 0, "start timestamp should be >= 0");
            ctx->copy_timestamp = true;
        }
        else if (!strcmp(arg, "--mvdump"))
        {
            ctx->dump_mv = true;
        }
        else if (!strcmp(arg, "--enc-cmd"))
        {
          ctx->b_use_enc_cmd = true;
        }
        else if (!strcmp(arg, "--blocking-mode"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->blocking_mode = atoi(*argp);
        }
        else if (!strcmp(arg, "-sf"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->startf = atoi(*argp);
        }
        else if (!strcmp(arg, "-ef"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->endf = atoi(*argp);
        }
        else if (!strcmp(arg, "-poc"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->poc_type = atoi(*argp);
        }
        else
        {
            CSV_PARSE_CHECK_ERROR(ctx->out_file_path, "Unknown option " << arg);
        }
    }
    if (ctx->externalRPS && ctx->RPS_threeLayerSvc)
    {
        ctx->rps_par.m_numTemperalLayers = 3;
        ctx->rps_par.nActiveRefFrames = 0;

        ctx->input_metadata = true;
        ctx->report_metadata = true;
        ctx->num_reference_frames = 2;
        ctx->iframe_interval = ctx->idr_interval = 32;
        if (ctx->encoder_pixfmt == V4L2_PIX_FMT_H264)
        {
            ctx->bGapsInFrameNumAllowed = false;
            ctx->nH264FrameNumBits = 8;
        }
        else if (ctx->encoder_pixfmt == V4L2_PIX_FMT_H265)
        {
            ctx->nH265PocLsbBits = 8;
        }
    }

    return 0;

error:
    print_help();
    return -1;
}
