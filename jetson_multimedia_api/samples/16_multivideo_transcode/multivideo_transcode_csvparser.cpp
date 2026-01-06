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

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <linux/v4l2-controls.h>
#include "multivideo_transcode.h"

#define CHECK_OPTION_VALUE(argp) if (!*argp || (*argp)[0] == '-') \
                                { \
                                    cerr << "Error: value not specified for option " << arg << endl; \
                                    goto error; \
                                }

#define CSV_PARSE_CHECK_ERROR(condition, str) \
    if (condition) {\
    cerr << "Error: " << str << endl; \
    goto error; \
    }

#define CHECK_IF_LAST_LOOP(i, num_files, argp, dec) if ( i + 1 < num_files ) \
                                { \
                                    argp-=dec; \
                                }

using namespace std;

static void
print_help(void)
{
    cerr << "\nmultivideo_transcode num_files <number of files> <in-file1> <in-pixfmt1> <out-file1> <out-pixfmt1> "
            "<in-file2> <in-pixfmt2> <out-file2> <out-pixfmt2> ... [OPTIONS]\n\n"
            "Supported Transcoding formats:\n"
            "\tH264\n"
            "\tH265\n"
            "\tVP8\n"
            "\tVP9\n\n"
            "TRANSCODER OPTIONS:\n"
            "\t-h,--help             Prints this text\n"
            "\t--dbg-level <level>   Sets the debug level [Values 0-3]\n"
            "\t--stats               Report profiling data for the app\n"
            "\t--max-perf            Enable maximum Performance \n"
            "\t--seek-mode           Seek to begin of input file without re-construct video codec when reach the "
            "end of input file for loop test (Only works with H264/H265)\n"
            "\t-ni <loop-count>      Number of iterations [Default = 1]\n\n"

            "DECODER OPTIONS:\n"
            "\t--input-nalu         Input to the decoder will be nal units\n"
            "\t--dec-report-metadata     Enable metadata reporting\n"
            "\t--dec-input-metadata  Enable metadata reporting for input header parsing error\n\n"

            "ENCODER OPTIONS: \n"
            "\t-br <bitrate>         Bitrate [Default = 4000000]\n"
            "\t-pbr <peak_bitrate>   Peak bitrate [Default = 1.2*bitrate]\n"
            "NOTE: Peak bitrate takes effect in VBR more; must be >= bitrate\n"
            "\t-p <profile>          Encoding Profile [Default = baseline]\n"
            "\t-l <level>            Encoding Level [Default set by the library]\n"
            "\t-rc <rate-control>    Ratecontrol mode [Default = cbr]\n"
            "\t--elossless           Enable Lossless encoding [Default = disabled,\n"
            "                        Option applicable only with YUV444 input and H264 encoder]\n"
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
            "\t--cd                  CABAC Disable for H264 [Default = disabled]\n"
            "\t-sir <interval>       Slice intrarefresh interval [Default = 0]\n\n"
            "\t-nbf <num>            Number of B frames [Default = 0]\n\n"
            "\t-goldcrc <string>     GOLD CRC\n\n"
            "\t--enc-report-metadata Print encoder output metadata\n"
            "\t--copy-timestamp <st> Enable copy timestamp with start timestamp(st) in seconds\n"
            "\t--mvdump              Dump encoded motion vectors\n\n"
            "\t-fnb <num_bits>       H264 FrameNum bits [Default = 0]\n\n"
            "\t-plb <num_bits>       H265 poc lsb bits [Default = 0]\n\n"
            "\t--noi                 Number of I-frames [Default = disabled]\n\n"
            "\t-poc <type>           Specify POC type [Default = 0]\n\n"
            "\t-MinQpI               Specify minimum Qp Value for I frame\n"
            "\t-MaxQpI               Specify maximum Qp Value for I frame\n"
            "\t-MinQpP               Specify minimum Qp Value for P frame\n"
            "\t-MaxQpP               Specify maximum Qp Value for P frame\n"
            "\t-MinQpB               Specify minimum Qp Value for B frame\n"
            "\t-MaxQpB               Specify maximum Qp Value for B frame\n\n"
            "NOTE: \n"
            "Supported Encoding profiles for H.264:\n"
            "\tbaseline\tmain\thigh\n"
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
            "Property ids:\n"
            "\tb<bitrate>  Bitrate\n"
            "\tp<peak_bitrate>  Peak Bitrate\n"
            "\tr<num/den>  Framerate\n"
            "\ti1          Force I-frame\n\n"
            "NOTE:"
            "\tOnly Blocking Mode is supported\n\n"
            "\tThe plane memory types as of now are hardcoded.\n"
            "\tDecoder: Output_plane = V4L2_MEMORY_MMAP & Capture_plane = V4L2_MEMORY_DMABUF \n"
            "\tEncoder: Output_plane = V4L2_MEMORY_DMABUF & Capture_plane = V4L2_MEMORY_MMAP\n";
}

static uint32_t
get_pixfmt(char *arg)
{
    if (!strcmp(arg, "H264"))
    {
        return V4L2_PIX_FMT_H264;
    }
    if (!strcmp(arg, "H265"))
    {
        return V4L2_PIX_FMT_H265;
    }
    if (!strcmp(arg, "VP8"))
    {
        return V4L2_PIX_FMT_VP8;
    }
    if (!strcmp(arg, "VP9"))
    {
        return V4L2_PIX_FMT_VP9;
    }
    return 0;
}

static int32_t
get_encoder_ratecontrol(char *arg)
{
    if (!strcmp(arg, "cbr"))
    {
        return V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
    }

    if (!strcmp(arg, "vbr"))
    {
        return V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
    }

    return -1;
}

static int32_t
get_encoder_profile_h264(char *arg)
{
    if (!strcmp(arg, "baseline"))
    {
        return V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
    }

    if (!strcmp(arg, "main"))
    {
        return V4L2_MPEG_VIDEO_H264_PROFILE_MAIN;
    }

    if (!strcmp(arg, "high"))
    {
        return V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
    }
    return -1;
}

static int32_t
get_encoder_profile_h265(char *arg)
{
    if (!strcmp(arg, "main"))
    {
        return V4L2_MPEG_VIDEO_H265_PROFILE_MAIN;
    }

    if (!strcmp(arg, "main10"))
    {
        return V4L2_MPEG_VIDEO_H265_PROFILE_MAIN10;
    }

    return -1;
}

static int32_t
get_h264_encoder_level(char *arg)
{
    if (!strcmp(arg, "1.0"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_0;
    }

    if (!strcmp(arg, "1b"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_1B;
    }

    if (!strcmp(arg, "1.1"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_1;
    }

    if (!strcmp(arg, "1.2"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_2;
    }

    if (!strcmp(arg, "1.3"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_3;
    }

    if (!strcmp(arg, "2.0"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_2_0;
    }

    if (!strcmp(arg, "2.1"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_2_1;
    }

    if (!strcmp(arg, "2.2"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_2_2;
    }

    if (!strcmp(arg, "3.0"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_3_0;
    }

    if (!strcmp(arg, "3.1"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_3_1;
    }

    if (!strcmp(arg, "3.2"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_3_2;
    }

    if (!strcmp(arg, "4.0"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_4_0;
    }

    if (!strcmp(arg, "4.1"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_4_1;
    }

    if (!strcmp(arg, "4.2"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_4_2;
    }

    if (!strcmp(arg, "5.0"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_5_0;
    }

    if (!strcmp(arg, "5.1"))
    {
        return V4L2_MPEG_VIDEO_H264_LEVEL_5_1;
    }

    return -1;
}

static int32_t
get_h265_encoder_level(char *arg)
{
    if (!strcmp(arg, "main1.0"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_1_0_MAIN_TIER;
    }

    if (!strcmp(arg, "high1.0"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_1_0_HIGH_TIER;
    }

    if (!strcmp(arg, "main2.0"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_2_0_MAIN_TIER;
    }

    if (!strcmp(arg, "high2.0"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_2_0_HIGH_TIER;
    }

    if (!strcmp(arg, "main2.1"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_2_1_MAIN_TIER;
    }

    if (!strcmp(arg, "high2.1"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_2_1_HIGH_TIER;
    }

    if (!strcmp(arg, "main3.0"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_3_0_MAIN_TIER;
    }

    if (!strcmp(arg, "high3.0"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_3_0_HIGH_TIER;
    }

    if (!strcmp(arg, "main3.1"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_3_1_MAIN_TIER;
    }

    if (!strcmp(arg, "high3.1"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_3_1_HIGH_TIER;
    }

    if (!strcmp(arg, "main4.0"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_4_0_MAIN_TIER;
    }

    if (!strcmp(arg, "high4.0"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_4_0_HIGH_TIER;
    }

    if (!strcmp(arg, "main4.1"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_4_1_MAIN_TIER;
    }

    if (!strcmp(arg, "high4.1"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_4_1_HIGH_TIER;
    }

    if (!strcmp(arg, "main5.0"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_0_MAIN_TIER;
    }

    if (!strcmp(arg, "high5.0"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_0_HIGH_TIER;
    }

    if (!strcmp(arg, "main5.1"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_1_MAIN_TIER;
    }

    if (!strcmp(arg, "high5.1"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_1_HIGH_TIER;
    }

    if (!strcmp(arg, "main5.2"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_2_MAIN_TIER;
    }

    if (!strcmp(arg, "high5.2"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_2_HIGH_TIER;
    }

    if (!strcmp(arg, "main6.0"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_0_MAIN_TIER;
    }

    if (!strcmp(arg, "high6.0"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_0_HIGH_TIER;
    }

    if (!strcmp(arg, "main6.1"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_1_MAIN_TIER;
    }

    if (!strcmp(arg, "high6.1"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_1_HIGH_TIER;
    }

    if (!strcmp(arg, "main6.2"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_2_MAIN_TIER;
    }

    if (!strcmp(arg, "high6.2"))
    {
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_2_HIGH_TIER;
    }

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
get_num_files(int argc, char *argv[])
{
    char **argp = argv;
    char *arg = *(++argp);
    int num_files;

    if (argc == 1 || (arg && (!strcmp(arg, "-h") || !strcmp(arg, "--help"))))
    {
        print_help();
        exit(EXIT_SUCCESS);
    }

    CSV_PARSE_CHECK_ERROR(argc < 3, "Insufficient arguments");

    if (!strcmp(arg, "num_files"))
    {
        argp++;
        num_files = atoi(*argp);
    }
    else
    {
        goto error;
    }

    return num_files;

error:
    print_help();
    return -1;
}

int
parse_csv_args(context_t ** ctx, int argc, char *argv[], int num_files)
{
    char **argp = argv;
    char *arg = *(++argp);
    int32_t intval = -1;

    if (argc == 1 || (arg && (!strcmp(arg, "-h") || !strcmp(arg, "--help"))))
    {
        print_help();
        exit(EXIT_SUCCESS);
    }

    CSV_PARSE_CHECK_ERROR(argc < 4, "Insufficient arguments");

    --argp;

    for (int i = 0; i < num_files ; i++ )
    {
        ctx[i]->in_file_path = strdup(*++argp);
        CSV_PARSE_CHECK_ERROR(!ctx[i]->in_file_path, "Input file not specified");

        ctx[i]->decoder_pixfmt = get_pixfmt(*(++argp));
        CSV_PARSE_CHECK_ERROR(ctx[i]->decoder_pixfmt == 0,
                              "Incorrect decoder type");

        ctx[i]->out_file_path = strdup(*(++argp));
        CSV_PARSE_CHECK_ERROR(!ctx[i]->out_file_path, "Output file not specified");

        ctx[i]->encoder_pixfmt = get_pixfmt(*(++argp));
        CSV_PARSE_CHECK_ERROR(ctx[i]->encoder_pixfmt == 0,
                              "Incorrect encoder type");
    }

    while ((arg = *(++argp)))
    {
        int i;
        for (i = 0 ; i < num_files ; i++)
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
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "--stats"))
            {
                ctx[i]->stats = true;
                ctx[i]->input_nalu = true;
            }
            else if (!strcmp(arg, "--disable-dpb"))
            {
                ctx[i]->disable_dpb = true;
            }
            else if (!strcmp(arg, "--dec-report-metadata"))
            {
                ctx[i]->dec_enable_metadata = true;
            }
            else if (!strcmp(arg, "--enc-report-metadata"))
            {
                ctx[i]->enc_report_metadata = true;
            }
            else if (!strcmp(arg, "--dec-input-metadata"))
            {
                ctx[i]->dec_input_metadata = true;
            }
            else if (!strcmp(arg, "--max-perf"))
            {
                ctx[i]->max_perf = 1;
            }
            else if (!strcmp(arg, "-fnb"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->nH264FrameNumBits = atoi(*argp);
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-plb"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->nH265PocLsbBits = atoi(*argp);
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "--noi"))
            {
                ctx[i]->bnoIframe = true;
            }
            else if (!strcmp(arg, "-fps"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->fps_n = atoi(*argp);
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->fps_d = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->fps_d == 0, "fps den should be > 0");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 2);
            }
            else if (!strcmp(arg, "--seek-mode"))
            {
                ctx[i]->seek_mode = true;
            }
            else if (!strcmp(arg, "-ni"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->num_iterations = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->num_iterations <= 0,
                        "nuber of iterations should be bigger than 0");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-br"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->bitrate = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->bitrate == 0, "bit rate should be > 0");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-pbr"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->peak_bitrate = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->peak_bitrate == 0, "bit rate should be > 0");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-ifi"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->iframe_interval = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->iframe_interval == 0,
                        "ifi size shoudl be > 0");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-idri"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->idr_interval = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->idr_interval == 0,
                        "idri size shoudl be > 0");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "--insert-spspps-idr"))
            {
                ctx[i]->insert_sps_pps_at_idr = true;
            }
            else if (!strcmp(arg, "--cd"))
            {
                ctx[i]->disable_cabac = true;
            }
            else if (!strcmp(arg, "--input-nalu"))
            {
                ctx[i]->input_nalu = true;
            }
            else if (!strcmp(arg, "--insert-vui"))
            {
                ctx[i]->insert_vui = true;
            }
            else if (!strcmp(arg, "--enable-extcolorfmt"))
            {
                ctx[i]->enable_extended_colorformat = true;
            }
            else if (!strcmp(arg, "--insert-aud"))
            {
                ctx[i]->insert_aud = true;
            }
            else if (!strcmp(arg, "--alliframes"))
            {
                ctx[i]->alliframes = true;
            }
            else if (!strcmp(arg, "-l"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                if (ctx[i]->encoder_pixfmt == V4L2_PIX_FMT_H264)
                {
                    ctx[i]->level = get_h264_encoder_level(*argp);
                }
                else if (ctx[i]->encoder_pixfmt == V4L2_PIX_FMT_H265)
                {
                    ctx[i]->level = get_h265_encoder_level(*argp);
                }
                CSV_PARSE_CHECK_ERROR(ctx[i]->level == (uint32_t)-1,
                        "Unsupported value for level: " << *argp);
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-rc"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                intval = get_encoder_ratecontrol(*argp);
                CSV_PARSE_CHECK_ERROR(intval == -1,
                        "Unsupported value for ratecontrol: " << *argp);
                ctx[i]->ratecontrol = (enum v4l2_mpeg_video_bitrate_mode) intval;
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "--elossless"))
            {
                ctx[i]->enable_lossless = true;
            }
            else if (!strcmp(arg, "-goldcrc"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                snprintf(ctx[i]->gold_crc,sizeof(ctx[i]->gold_crc),"%s", *argp);
                ctx[i]->use_gold_crc = true;
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-p"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                if (ctx[i]->encoder_pixfmt == V4L2_PIX_FMT_H264)
                {
                    ctx[i]->profile = get_encoder_profile_h264(*argp);
                }
                else if (ctx[i]->encoder_pixfmt == V4L2_PIX_FMT_H265)
                {
                    ctx[i]->profile = get_encoder_profile_h265(*argp);
                }
                CSV_PARSE_CHECK_ERROR(ctx[i]->profile == (uint32_t) -1,
                            "Unsupported value for profile: " << *argp);
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-tt"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->temporal_tradeoff_level =
                    (enum v4l2_enc_temporal_tradeoff_level_type) atoi(*argp);
                CSV_PARSE_CHECK_ERROR(
                        (ctx[i]->temporal_tradeoff_level <
                         V4L2_ENC_TEMPORAL_TRADEOFF_LEVEL_DROPNONE ||
                         ctx[i]->temporal_tradeoff_level >
                         V4L2_ENC_TEMPORAL_TRADEOFF_LEVEL_DROP2IN3),
                        "Unsupported value for temporal tradeoff: " << *argp);
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-slt"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                switch (atoi(*argp))
                {
                    case 1:
                        ctx[i]->slice_length_type = V4L2_ENC_SLICE_LENGTH_TYPE_MBLK;
                        break;
                    case 2:
                        ctx[i]->slice_length_type = V4L2_ENC_SLICE_LENGTH_TYPE_BITS;
                        break;
                    default:
                        CSV_PARSE_CHECK_ERROR(true,
                                "Unsupported value for slice length type: " << *argp);
                }
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-hpt"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                switch (atoi(*argp))
                {
                    case 1:
                        ctx[i]->hw_preset_type = V4L2_ENC_HW_PRESET_ULTRAFAST;
                        break;
                    case 2:
                        ctx[i]->hw_preset_type = V4L2_ENC_HW_PRESET_FAST;
                        break;
                    case 3:
                        ctx[i]->hw_preset_type = V4L2_ENC_HW_PRESET_MEDIUM;
                        break;
                    case 4:
                        ctx[i]->hw_preset_type = V4L2_ENC_HW_PRESET_SLOW;
                        break;
                    default:
                        CSV_PARSE_CHECK_ERROR(true,
                                "Unsupported value for encoder HW Preset Type: " << *argp);
                }
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-slen"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->slice_length = (uint32_t) atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->slice_length == 0,
                        "Slice length should be > 0");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-vbs"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->virtual_buffer_size = (uint32_t) atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->virtual_buffer_size == 0,
                        "Virtual buffer size should be > 0");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-nbf"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->num_b_frames = (uint32_t) atoi(*argp);
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-nrf"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->num_reference_frames = (uint32_t) atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->num_reference_frames == 0,
                        "Num reference frames should be > 0");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "--color-space"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                int num = (uint32_t) atoi(*argp);
                switch(num)
                {
                    case 1  :ctx[i]->cs = V4L2_COLORSPACE_SMPTE170M;
                             break;
                    case 2  :ctx[i]->cs = V4L2_COLORSPACE_REC709;
                             break;
                }
                CSV_PARSE_CHECK_ERROR(!(num>0&&num<3),
                        "Color space should be > 0 and < 4");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-sir"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->slice_intrarefresh_interval = (uint32_t) atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->slice_intrarefresh_interval == 0,
                        "Slice intrarefresh interval should be > 0");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-MinQpI"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->nMinQpI = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->nMinQpI > 51, "Min Qp should be >= 0 and <= 51");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-MaxQpI"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->nMaxQpI = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->nMaxQpI > 51, "Max Qp should be >= 0 and <= 51");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-MinQpP"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->nMinQpP = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->nMinQpP > 51, "Min Qp should be >= 0 and <= 51");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-MaxQpP"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->nMaxQpP = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->nMaxQpP > 51, "Max Qp should be >= 0 and <= 51");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-MinQpB"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->nMinQpB = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->nMinQpB > 51, "Min Qp should be >= 0 and <= 51");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-MaxQpB"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->nMaxQpB = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->nMaxQpB > 51, "Max Qp should be >= 0 and <= 51");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "--copy-timestamp"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->start_ts = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->start_ts < 0, "start timestamp should be >= 0");
                ctx[i]->copy_timestamp = true;
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "--mvdump"))
            {
                ctx[i]->dump_mv = true;
            }
            else if (!strcmp(arg, "--enc-cmd"))
            {
              ctx[i]->b_use_enc_cmd = true;
            }
            else if (!strcmp(arg, "--blocking-mode"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->blocking_mode = atoi(*argp);
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-poc"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->poc_type = atoi(*argp);
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else
            {
                CSV_PARSE_CHECK_ERROR(ctx[i]->out_file_path, "Unknown option " << arg);
            }
        }
    }

    return 0;

error:
    print_help();
    return -1;
}
