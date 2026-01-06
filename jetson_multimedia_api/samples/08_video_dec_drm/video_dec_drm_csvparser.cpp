/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION. All rights reserved.
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

#include "video_dec_drm.h"

#define CHECK_OPTION_VALUE(argp) \
    if(!*argp || (*argp)[0] == '-') \
    { \
        cerr << "Error: value not specified for option " << arg << endl; \
        goto error; \
    }

#define CSV_PARSE_CHECK_ERROR(condition, str) \
    if (condition) \
    {\
        cerr << "Error: " << str << endl; \
        goto error; \
    }

using namespace std;

static void
print_help(void)
{
    cerr << "\n\tUsage:\n"
            "\t\tvideo_decode_drm [options]\n\n"
            "\tSupported video formats:\n"
            "\t\tH264\n"
            "\t\tH265\n\n"
            "\tExamples:\n"
            "\t\t./video_dec_drm --disable-video\n"
            "\t\t./video_dec_drm ../../data/Video/sample_outdoor_car_1080p_10fps.h264 H264 --disable-ui\n"
            "\t\t./video_dec_drm ../../data/Video/sample_outdoor_car_1080p_10fps.h264 H264\n\n"
            "\tOPTIONS:\n"
            "\t\t-h,--help            Prints this text\n"
            "\t\t--dbg-level <level>  Sets the debug level [Values 0-3]\n"
            "\t\t--stats              Report profiling data for the app\n"
            "\t\t--disable-ui         Disable ui stream\n"
            "\t\t--disable-video      Disable video stream\n"
            "\t\t-crtc <index>        Display crtc index [Default = 0]\n"
            "\t\t-connector <index>   Display connector index [Default = 0]\n"
            "\t\t-ww <width>          Video width in pixels [Default = video-width]\n"
            "\t\t-wh <height>         Video height in pixels [Default = video-height]\n"
            "\t\t-wx <x-offset>       Video horizontal offset [Default = 0]\n"
            "\t\t-wy <y-offset>       Video vertical offset [Default = 0]\n"
            "\t\t-fps <fps>           Display rate in frames per second [Default = 30]\n"
            "\t\t-s <iteration>       Iteration of stress test [Default = 0]\n"
            "\t\t-co <colorspace>     Set colorspace conversion after decode\n"
            "\t\t                     0 = BT601, 1 = BT709, 2 = BT2020 [Default = 0]\n"
            "\t\t-o <out-file>        Write to output file\n"
            "\n";
}

static uint32_t
get_decoder_type(char *arg)
{
    if (!strcmp(arg, "H264"))
        return V4L2_PIX_FMT_H264;
    if (!strcmp(arg, "H265"))
        return V4L2_PIX_FMT_H265;
    return 0;
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
    uint32_t colorspace;

    if (argc == 1 || (arg && (!strcmp(arg, "-h") || !strcmp(arg, "--help"))))
    {
        print_help();
        exit(EXIT_SUCCESS);
    }

    if (arg && !strcmp(arg, "--disable-video"))
    {
        ctx->disable_video = true;
    }
    else
    {
        CSV_PARSE_CHECK_ERROR(argc < 3, "Insufficient arguments");

        ctx->in_file_path = strdup(*argp);
        CSV_PARSE_CHECK_ERROR(!ctx->in_file_path, "Input file not specified");

        ctx->decoder_pixfmt = get_decoder_type(*(++argp));
        CSV_PARSE_CHECK_ERROR(ctx->decoder_pixfmt == 0,
                "Incorrect input format");
    }

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
        else if (!strcmp(arg, "--disable-ui"))
        {
            ctx->disable_ui = true;
            CSV_PARSE_CHECK_ERROR((ctx->disable_video),
                                    "Couldn't disable both ui and video stream)");
        }
        else if (!strcmp(arg, "--disable-video"))
        {
            ctx->disable_video = true;
            CSV_PARSE_CHECK_ERROR((ctx->disable_ui),
                                    "Couldn't disable both video and ui stream)");
        }
        else if (!strcmp(arg, "-crtc"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->crtc = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->crtc > 1,
                                  "Display crtc index should be 0 or 1");
        }
        else if (!strcmp(arg, "-connector"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->connector = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->connector > 1,
                                  "Display connector index should be 0 or 1");
        }
        else if (!strcmp(arg, "-wh"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->window_height = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->window_height == 0,
                                  "Window height should be > 0");
        }
        else if (!strcmp(arg, "-ww"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->window_width = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->window_width == 0,
                                  "Window width should be > 0");
        }
        else if (!strcmp(arg, "-wx"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->window_x = atoi(*argp);
        }
        else if (!strcmp(arg, "-wy"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->window_y = atoi(*argp);
        }
        else if (!strcmp(arg, "-fps"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->fps = atof(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->fps == 0, "FPS should be > 0");
        }
        else if (!strcmp(arg, "-s"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->stress_iteration = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->disable_video),
                                    "Doesn't support to stress only ui stream");
        }
        else if (!strcmp(arg, "-o"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->out_file_path = strdup(*argp);
            CSV_PARSE_CHECK_ERROR(!ctx->out_file_path,
                                  "Output file not specified");
        }
        else if (!strcmp(arg, "-co"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            colorspace = atoi(*argp);
            if (colorspace == 1)
                ctx->conv_out_colorspace = V4L2_COLORSPACE_REC709;
            else if (colorspace == 2)
                ctx->conv_out_colorspace = V4L2_COLORSPACE_BT2020;
            else
                ctx->conv_out_colorspace = V4L2_COLORSPACE_SMPTE170M;
            CSV_PARSE_CHECK_ERROR((colorspace < 0 || colorspace > 2),
                                    "converter output colorspace shoud be 0(BT601) 1(BT709), 2(BT2020)");
        }
        else
        {
            goto error;
        }
    }

    return 0;

error:
    print_help();
    return -1;
}
