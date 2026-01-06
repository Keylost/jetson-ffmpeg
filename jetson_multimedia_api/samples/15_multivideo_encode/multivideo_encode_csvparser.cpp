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

#include <iostream>
#include <cstring>
#include <linux/v4l2-controls.h>
#include "multivideo_encode.h"

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
    cerr << "\nmultivideo_encode num_files <number_of_files> "
            "<in-file1> <in-width1> <in-height1> <encoder-type1> <out-file1> "
            "<in-file2> <in-width2> <in-height2> <encoder-type2> <out-file2> "
            "[OPTIONS]\n\n"
            "Encoder Types:\n"
            "\tH264\n"
            "\tH265\n"
            "\tVP8\n"
            "\tVP9\n\n"
            "OPTIONS:\n"
            "\t-h,--help             Prints this text\n"
            "\t-br <bitrate>         Bitrate [Default = 4000000]\n"
            "\t-pbr <peak_bitrate>   Peak bitrate [Default = 1.2*bitrate]\n\n"
            "NOTE: Peak bitrate takes effect in VBR more; must be >= bitrate\n\n"
            "\t-p <profile>          Encoding Profile [Default = baseline]\n"
            "\t-l <level>            Encoding Level [Default set by the library]\n"
            "\t-rc <rate-control>    Ratecontrol mode [Default = cbr]\n"
            "\t--elossless           Enable Lossless encoding [Default = disabled,"
                                     "Option applicable only with YUV444 input and H264 encoder]\n"
            "\t-ifi <interval>       I-frame Interval [Default = 30]\n"
            "\t-idri <interval>      IDR Interval [Default = 256]\n"
            "\t--insert-vui          Insert VUI [Default = disabled]\n"
            "\t--enable-extcolorfmt  Set Extended ColorFormat (Only works with insert-vui) [Default = disabled]\n"
            "\t-fps <num> <den>      Encoding fps in num/den [Default = 30/1]\n\n"
            "\t-nrf <num>            Number of reference frames [Default = 1]\n\n"
            "\t-hpt <type>           HW preset type (1 = ultrafast, 2 = fast, 3 = medium,  4 = slow)\n"
            "\t--blocking-mode <val> Set blocking mode, 0 is non-blocking, 1 for blocking (Default) \n\n"
            "\t--mvdump              Dump encoded motion vectors to <out-file>_mvdump\n\n"
            "\t-mem_type_oplane <num> Specify memory type for the output plane to be used [1 = V4L2_MEMORY_MMAP, 2 = V4L2_MEMORY_USERPTR, 3 = V4L2_MEMORY_DMABUF]\n\n"
            "\t-s <loop-count>       Stress test [Default = 1]\n\n"
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
            "\tcbr\tvbr\n\n";
}

static uint32_t
get_encoder_type(char *arg)
{
    if (!strcmp (arg, "H264"))
        return V4L2_PIX_FMT_H264;
    if (!strcmp (arg, "H265"))
        return V4L2_PIX_FMT_H265;
    if (!strcmp (arg, "VP8"))
        return V4L2_PIX_FMT_VP8;
    if (!strcmp (arg, "VP9"))
        return V4L2_PIX_FMT_VP9;
    return 0;
}

static int32_t
get_encoder_ratecontrol(char *arg)
{
    if (!strcmp (arg, "cbr"))
        return V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;

    if (!strcmp (arg, "vbr"))
        return V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;

    return -1;
}

static int32_t
get_encoder_profile_h264(char *arg)
{
    if (!strcmp (arg, "baseline"))
        return V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;

    if (!strcmp (arg, "main"))
        return V4L2_MPEG_VIDEO_H264_PROFILE_MAIN;

    if (!strcmp (arg, "high"))
        return V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
    return -1;
}

static int32_t
get_encoder_profile_h265(char *arg)
{
    if (!strcmp (arg, "main"))
        return V4L2_MPEG_VIDEO_H265_PROFILE_MAIN;

    if (!strcmp (arg, "main10"))
        return V4L2_MPEG_VIDEO_H265_PROFILE_MAIN10;

    return -1;
}

static int32_t
get_h264_encoder_level(char *arg)
{
    if (!strcmp (arg, "1.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_0;

    if (!strcmp (arg, "1b"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1B;

    if (!strcmp (arg, "1.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_1;

    if (!strcmp (arg, "1.2"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_2;

    if (!strcmp (arg, "1.3"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_3;

    if (!strcmp (arg, "2.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_2_0;

    if (!strcmp (arg, "2.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_2_1;

    if (!strcmp (arg, "2.2"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_2_2;

    if (!strcmp (arg, "3.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_3_0;

    if (!strcmp (arg, "3.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_3_1;

    if (!strcmp (arg, "3.2"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_3_2;

    if (!strcmp (arg, "4.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_4_0;

    if (!strcmp (arg, "4.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_4_1;

    if (!strcmp (arg, "4.2"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_4_2;

    if (!strcmp (arg, "5.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_5_0;

    if (!strcmp (arg, "5.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_5_1;

    return -1;
}

static int32_t
get_h265_encoder_level(char *arg)
{
    if (!strcmp (arg, "main1.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_1_0_MAIN_TIER;

    if (!strcmp (arg, "high1.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_1_0_HIGH_TIER;

    if (!strcmp (arg, "main2.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_2_0_MAIN_TIER;

    if (!strcmp (arg, "high2.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_2_0_HIGH_TIER;

    if (!strcmp (arg, "main2.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_2_1_MAIN_TIER;

    if (!strcmp (arg, "high2.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_2_1_HIGH_TIER;

    if (!strcmp (arg, "main3.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_3_0_MAIN_TIER;

    if (!strcmp (arg, "high3.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_3_0_HIGH_TIER;

    if (!strcmp (arg, "main3.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_3_1_MAIN_TIER;

    if (!strcmp (arg, "high3.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_3_1_HIGH_TIER;

    if (!strcmp (arg, "main4.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_4_0_MAIN_TIER;

    if (!strcmp (arg, "high4.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_4_0_HIGH_TIER;

    if (!strcmp (arg, "main4.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_4_1_MAIN_TIER;

    if (!strcmp (arg, "high4.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_4_1_HIGH_TIER;

    if (!strcmp (arg, "main5.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_0_MAIN_TIER;

    if (!strcmp (arg, "high5.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_0_HIGH_TIER;

    if (!strcmp (arg, "main5.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_1_MAIN_TIER;

    if (!strcmp (arg, "high5.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_1_HIGH_TIER;

    if (!strcmp (arg, "main5.2"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_2_MAIN_TIER;

    if (!strcmp (arg, "high5.2"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_5_2_HIGH_TIER;

    if (!strcmp (arg, "main6.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_0_MAIN_TIER;

    if (!strcmp (arg, "high6.0"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_0_HIGH_TIER;

    if (!strcmp (arg, "main6.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_1_MAIN_TIER;

    if (!strcmp (arg, "high6.1"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_1_HIGH_TIER;

    if (!strcmp (arg, "main6.2"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_2_MAIN_TIER;

    if (!strcmp (arg, "high6.2"))
        return V4L2_MPEG_VIDEO_H265_LEVEL_6_2_HIGH_TIER;

    return -1;
}

int
get_num_files(int argc, char *argv[])
{
    char **argp = argv;
    char *arg = *(++argp);
    int num_files;

    if (argc == 1 || (arg && (!strcmp (arg, "-h") || !strcmp (arg, "--help"))))
    {
        print_help();
        exit(EXIT_SUCCESS);
    }

    CSV_PARSE_CHECK_ERROR (argc < 3, "Insufficient arguments");

    if (!strcmp (arg, "num_files"))
    {
        argp++;
        num_files = atoi (*argp);
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

    if (argc == 1 || (arg && (!strcmp (arg, "-h") || !strcmp (arg, "--help"))))
    {
        print_help();
        exit(EXIT_SUCCESS);
    }

    CSV_PARSE_CHECK_ERROR (argc < 5*num_files, "Insufficient arguments");

    --argp;
    for (int i = 0; i < num_files; i++)
    {
        ctx[i]->in_file_path = *(++argp);
        CSV_PARSE_CHECK_ERROR (ctx[i]->in_file_path.empty(),
                              "Input file not specified");

        ctx[i]->width = atoi (*(++argp));
        CSV_PARSE_CHECK_ERROR (ctx[i]->width == 0, "Input width should be > 0");

        ctx[i]->height = atoi (*(++argp));
        CSV_PARSE_CHECK_ERROR (ctx[i]->height == 0, "Input height should be > 0");

        ctx[i]->encoder_pixfmt = get_encoder_type(*(++argp));
        CSV_PARSE_CHECK_ERROR (ctx[i]->encoder_pixfmt == 0,
                              "Incorrect encoder type");

        ctx[i]->out_file_path = *(++argp);
        CSV_PARSE_CHECK_ERROR (ctx[i]->out_file_path.empty(),
                              "Output file not specified");
    }

    while ( (arg = *(++argp)) )
    {
        if (!strcmp (arg, "-h") || !strcmp (arg, "--help"))
        {
            print_help();
            exit(EXIT_SUCCESS);
        }
        else if (!strcmp (arg, "-br"))
        {
            argp++;
            CHECK_OPTION_VALUE (argp);
            uint32_t bitrate = atoi (*argp);
            for (int i = 0; i < num_files; i++)
            {
                ctx[i]->bitrate = bitrate;
                CSV_PARSE_CHECK_ERROR (ctx[i]->bitrate == 0, "bit rate should be > 0");
            }
        }
        else if (!strcmp (arg, "-pbr"))
        {
            argp++;
            CHECK_OPTION_VALUE (argp);
            uint32_t peak_bitrate = atoi (*argp);
            for (int i = 0; i < num_files; i++)
            {
                ctx[i]->peak_bitrate = peak_bitrate;
                CSV_PARSE_CHECK_ERROR (ctx[i]->peak_bitrate == 0,
                                      "bit rate should be > 0");
            }
        }
        else if (!strcmp (arg, "-p"))
        {
            argp++;
            CHECK_OPTION_VALUE (argp);
            for (int i = 0; i < num_files; i++)
            {
                if (ctx[i]->encoder_pixfmt == V4L2_PIX_FMT_H264)
                {
                    ctx[i]->profile = get_encoder_profile_h264 (*argp);
                }
                else if (ctx[i]->encoder_pixfmt == V4L2_PIX_FMT_H265)
                {
                    ctx[i]->profile = get_encoder_profile_h265 (*argp);
                }
                CSV_PARSE_CHECK_ERROR (ctx[i]->profile == (uint32_t) -1,
                            "Unsupported value for profile: " << *argp);
            }
        }
        else if (!strcmp (arg, "-l"))
        {
            argp++;
            CHECK_OPTION_VALUE (argp);
            for (int i = 0; i < num_files; i++)
            {
                if (ctx[i]->encoder_pixfmt == V4L2_PIX_FMT_H264)
                {
                    ctx[i]->level = get_h264_encoder_level (*argp);
                }
                else if (ctx[i]->encoder_pixfmt == V4L2_PIX_FMT_H265)
                {
                    ctx[i]->level = get_h265_encoder_level (*argp);
                }
                CSV_PARSE_CHECK_ERROR (ctx[i]->level == (uint32_t)-1,
                        "Unsupported value for level: " << *argp);
            }
        }
        else if (!strcmp (arg, "-rc"))
        {
            argp++;
            CHECK_OPTION_VALUE (argp);
            intval = get_encoder_ratecontrol (*argp);
            CSV_PARSE_CHECK_ERROR (intval == -1,
                    "Unsupported value for ratecontrol: " << *argp);
            for (int i = 0; i < num_files; i++)
            {
                ctx[i]->ratecontrol = (enum v4l2_mpeg_video_bitrate_mode) intval;
            }
        }
        else if (!strcmp (arg, "--elossless"))
        {
            for (int i = 0; i < num_files; i++)
            {
                ctx[i]->enableLossless = true;
            }
        }
        else if (!strcmp (arg, "-ifi"))
        {
            argp++;
            CHECK_OPTION_VALUE (argp);
            uint32_t iframe_interval = atoi (*argp);
            for (int i = 0; i < num_files; i++)
            {
                ctx[i]->iframe_interval = iframe_interval;
                CSV_PARSE_CHECK_ERROR (ctx[i]->iframe_interval == 0,
                                      "ifi size shoudl be > 0");
            }
        }
        else if (!strcmp (arg, "-idri"))
        {
            argp++;
            CHECK_OPTION_VALUE (argp);
            uint32_t idr_interval = atoi (*argp);
            for (int i = 0; i < num_files; i++)
            {
                ctx[i]->idr_interval = idr_interval;
                CSV_PARSE_CHECK_ERROR (ctx[i]->idr_interval == 0,
                                      "idri size shoudl be > 0");
            }
        }
        else if (!strcmp (arg, "--insert-vui"))
        {
            for (int i = 0; i < num_files; i++)
            {
                ctx[i]->insert_vui = true;
            }
        }
        else if (!strcmp (arg, "--enable-extcolorfmt"))
        {
            for (int i = 0; i < num_files; i++)
            {
                ctx[i]->enable_extended_colorformat = true;
            }
        }
        else if (!strcmp (arg, "-fps"))
        {
            argp++;
            CHECK_OPTION_VALUE (argp);
            uint32_t fps_n = atoi (*argp);
            for (int i = 0; i < num_files; i++)
                ctx[i]->fps_n = fps_n;
            argp++;
            CHECK_OPTION_VALUE (argp);
            uint32_t fps_d = atoi (*argp);
            for (int i = 0; i < num_files; i++)
            {
                ctx[i]->fps_d = fps_d;
                CSV_PARSE_CHECK_ERROR (ctx[i]->fps_d == 0, "fps den should be > 0");
            }
        }
        else if (!strcmp(arg, "-hpt"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            switch (atoi(*argp))
            {
                case 1:
                    ctx[0]->hw_preset_type = V4L2_ENC_HW_PRESET_ULTRAFAST;
                    break;
                case 2:
                    ctx[0]->hw_preset_type = V4L2_ENC_HW_PRESET_FAST;
                    break;
                case 3:
                    ctx[0]->hw_preset_type = V4L2_ENC_HW_PRESET_MEDIUM;
                    break;
                case 4:
                    ctx[0]->hw_preset_type = V4L2_ENC_HW_PRESET_SLOW;
                    break;
                default:
                    CSV_PARSE_CHECK_ERROR(true,
                            "Unsupported value for encoder HW Preset Type: " << *argp);
            }
            for (int i = 1; i < num_files; i++)
                ctx[i]->hw_preset_type = ctx[0]->hw_preset_type;
        }
        else if (!strcmp (arg, "-nrf"))
        {
            argp++;
            CHECK_OPTION_VALUE (argp);
            uint32_t num_reference_frames = (uint32_t) atoi (*argp);
            for (int i = 0; i < num_files; i++)
            {
                ctx[i]->num_reference_frames = num_reference_frames;
                CSV_PARSE_CHECK_ERROR (ctx[i]->num_reference_frames == 0,
                                      "Num reference frames should be > 0");
            }
        }
        else if (!strcmp (arg, "--blocking-mode"))
        {
            argp++;
            CHECK_OPTION_VALUE (argp);
            bool blocking_mode = atoi (*argp);
            for (int i = 0; i < num_files; i++)
            {
                ctx[i]->blocking_mode = blocking_mode;
            }
        }
        else if (!strcmp (arg, "--mvdump"))
        {
            for (int i = 0; i < num_files; i++)
            {
                ctx[i]->dump_mv = true;
            }
        }
        else if (!strcmp (arg, "-mem_type_oplane"))
        {
            argp++;
            CHECK_OPTION_VALUE (argp);
            int num = (uint32_t) atoi (*argp);
            for (int i = 0; i < num_files; i++)
            {
                switch(num)
                {
                    case 1  :ctx[i]->output_memory_type = V4L2_MEMORY_MMAP;
                            break;
                    case 2  :ctx[i]->output_memory_type = V4L2_MEMORY_USERPTR;
                            break;
                    case 3  :ctx[i]->output_memory_type = V4L2_MEMORY_DMABUF;
                            break;
                }
                CSV_PARSE_CHECK_ERROR (!(num > 0 && num < 4),
                        "Memory type selection should be > 0 and < 4");
            }
        }
        else if (!strcmp (arg, "-s"))
        {
            argp++;
            CHECK_OPTION_VALUE (argp);
            int stress_test = atoi (*argp);
            for (int i = 0; i < num_files; i++)
            {
                ctx[i]->stress_test = stress_test;
                CSV_PARSE_CHECK_ERROR (ctx[i]->stress_test <= 0,
                                       "stress times should be bigger than 0");
            }
        }
        else
        {
            cerr << "Error: " << "Unknown option " << arg << endl;
            goto error;
        }
    }

    return 0;

error:
    print_help();
    return -1;
}