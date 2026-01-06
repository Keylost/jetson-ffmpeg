/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION. All rights reserved.
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

#include "video_convert.h"

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
    cerr <<
        "\nvideo_convert <in-file> <in-width> <in-height> <in-format> <out-file-prefix> <out-width> <out-height> <out-format> [OPTIONS]\n\n"
        "Supported formats:\n"
        "\tYUV420\n"
        "\tYUV420_ER\n"
        "\tNV12\n"
        "\tNV12_ER\n"
        "\tNV21\n"
        "\tNV21_ER\n"
        "\tUYVY\n"
        "\tUYVY_ER\n"
        "\tVYUY\n"
        "\tVYUY_ER\n"
        "\tYUYV\n"
        "\tYUYV_ER\n"
        "\tYVYU\n"
        "\tYVYU_ER\n"
        "\tABGR32\n"
        "\tXRGB32\n"
        "\tARGB32\n"
        "\tNV12_10LE\n"
        "\tNV12_10LE_709\n"
        "\tNV12_10LE_709_ER\n"
        "\tNV12_10LE_2020\n"
        "\tNV21_10LE\n"
        "\tNV12_12LE\n"
        "\tNV12_12LE_2020\n"
        "\tNV21_12LE\n"
        "\tYUV420_709\n"
        "\tYUV420_709_ER\n"
        "\tNV12_709\n"
        "\tNV12_709_ER\n"
        "\tYUV420_2020\n"
        "\tNV12_2020\n"
        "\tYUV444\n"
        "\tGRAY8\n"
        "\tNV16\n"
        "\tNV16_10LE\n"
        "\tNV24\n"
        "\tNV16_ER\n"
        "\tNV24_ER\n"
        "\tNV16_709\n"
        "\tNV24_709\n"
        "\tNV16_709_ER\n"
        "\tNV24_709_ER\n"
        "\tNV24_10LE_709\n"
        "\tNV24_10LE_709_ER\n"
        "\tNV24_10LE_2020\n"
        "\tNV24_12LE_2020\n\n"
        "OPTIONS:\n"
        "\t-h,--help            Prints this text\n\n"
        "\t-a,--async           Sets the Transform mode to Asynchronous(Non-Blocking) [Default = Blocking]\n\n"
        "\t-t,--num-thread <number>     Number of thread to process [Default = 1]\n"
        "\t-s,--create-session  Create seperate session for each thread\n"
        "\t-p,--perf            Calculate performance\n"
        "\t-cr <left> <top> <width> <height> Set the cropping rectangle [Default = 0 0 0 0]\n"
        "\t-fm <method>         Flip method to use [Default = 0]\n"
        "\t-im <method>         Interpolation method to use [Default = 1]\n\n"
        "Allowed values for flip method:\n"
        "0 = Identity(no rotation)\n"
        "1 = 90 degree counter-clockwise rotation\n"
        "2 = 180 degree counter-clockwise rotation\n"
        "3 = 270 degree counter-clockwise rotation\n"
        "4 = Horizontal flip\n"
        "5 = Vertical flip\n"
        "6 = Flip across upper left/lower right diagonal\n"
        "7 = Flip across upper right/lower left diagonal\n\n"
        "Allowed values for interpolation method:\n"
        "0 = nearest    1 = bilinear\n"
        "2 = 5-tap      3 = 10-tap\n"
        "4 = smart      5 = nicest\n\n";
}

static NvBufferColorFormat
get_color_format(const char* userdefined_fmt)
{
    if (!strcmp(userdefined_fmt, "YUV420"))
        return NvBufferColorFormat_YUV420;
    if (!strcmp(userdefined_fmt, "YUV420_ER"))
        return NvBufferColorFormat_YUV420_ER;
    if (!strcmp(userdefined_fmt, "NV12"))
        return NvBufferColorFormat_NV12;
    if (!strcmp(userdefined_fmt, "NV12_ER"))
        return NvBufferColorFormat_NV12_ER;
    if (!strcmp(userdefined_fmt, "NV21"))
        return NvBufferColorFormat_NV21;
    if (!strcmp(userdefined_fmt, "NV21_ER"))
        return NvBufferColorFormat_NV21_ER;
    if (!strcmp(userdefined_fmt, "UYVY"))
        return NvBufferColorFormat_UYVY;
    if (!strcmp(userdefined_fmt, "UYVY_ER"))
        return NvBufferColorFormat_UYVY_ER;
    if (!strcmp(userdefined_fmt, "VYUY"))
        return NvBufferColorFormat_VYUY;
    if (!strcmp(userdefined_fmt, "VYUY_ER"))
        return NvBufferColorFormat_VYUY_ER;
    if (!strcmp(userdefined_fmt, "YUYV"))
        return NvBufferColorFormat_YUYV;
    if (!strcmp(userdefined_fmt, "YUYV_ER"))
        return NvBufferColorFormat_YUYV_ER;
    if (!strcmp(userdefined_fmt, "YVYU"))
        return NvBufferColorFormat_YVYU;
    if (!strcmp(userdefined_fmt, "YVYU_ER"))
        return NvBufferColorFormat_YVYU_ER;
    if (!strcmp(userdefined_fmt, "ABGR32"))
        return NvBufferColorFormat_ABGR32;
    if (!strcmp(userdefined_fmt, "XRGB32"))
        return NvBufferColorFormat_XRGB32;
    if (!strcmp(userdefined_fmt, "ARGB32"))
        return NvBufferColorFormat_ARGB32;
    if (!strcmp(userdefined_fmt, "NV12_10LE"))
        return NvBufferColorFormat_NV12_10LE;
    if (!strcmp(userdefined_fmt, "NV12_10LE_709"))
        return NvBufferColorFormat_NV12_10LE_709;
    if (!strcmp(userdefined_fmt, "NV12_10LE_709_ER"))
        return NvBufferColorFormat_NV12_10LE_709_ER;
    if (!strcmp(userdefined_fmt, "NV12_10LE_2020"))
        return NvBufferColorFormat_NV12_10LE_2020;
    if (!strcmp(userdefined_fmt, "NV21_10LE"))
        return NvBufferColorFormat_NV21_10LE;
    if (!strcmp(userdefined_fmt, "NV12_12LE"))
        return NvBufferColorFormat_NV12_12LE;
    if (!strcmp(userdefined_fmt, "NV12_12LE_2020"))
        return NvBufferColorFormat_NV12_12LE_2020;
    if (!strcmp(userdefined_fmt, "NV21_12LE"))
        return NvBufferColorFormat_NV21_12LE;
    if (!strcmp(userdefined_fmt, "YUV420_709"))
        return NvBufferColorFormat_YUV420_709;
    if (!strcmp(userdefined_fmt, "YUV420_709_ER"))
        return NvBufferColorFormat_YUV420_709_ER;
    if (!strcmp(userdefined_fmt, "NV12_709"))
        return NvBufferColorFormat_NV12_709;
    if (!strcmp(userdefined_fmt, "NV12_709_ER"))
        return NvBufferColorFormat_NV12_709_ER;
    if (!strcmp(userdefined_fmt, "YUV420_2020"))
        return NvBufferColorFormat_YUV420_2020;
    if (!strcmp(userdefined_fmt, "NV12_2020"))
        return NvBufferColorFormat_NV12_2020;
    if (!strcmp(userdefined_fmt, "YUV444"))
        return NvBufferColorFormat_YUV444;
    if (!strcmp(userdefined_fmt, "GRAY8"))
        return NvBufferColorFormat_GRAY8;
    if (!strcmp(userdefined_fmt, "NV16"))
        return NvBufferColorFormat_NV16;
    if (!strcmp(userdefined_fmt, "NV16_10LE"))
        return NvBufferColorFormat_NV16_10LE;
    if (!strcmp(userdefined_fmt, "NV24"))
        return NvBufferColorFormat_NV24;
    if (!strcmp(userdefined_fmt, "NV16_ER"))
        return NvBufferColorFormat_NV16_ER;
    if (!strcmp(userdefined_fmt, "NV24_ER"))
        return NvBufferColorFormat_NV24_ER;
    if (!strcmp(userdefined_fmt, "NV16_709"))
        return NvBufferColorFormat_NV16_709;
    if (!strcmp(userdefined_fmt, "NV24_709"))
        return NvBufferColorFormat_NV24_709;
    if (!strcmp(userdefined_fmt, "NV16_709_ER"))
        return NvBufferColorFormat_NV16_709_ER;
    if (!strcmp(userdefined_fmt, "NV24_709_ER"))
        return NvBufferColorFormat_NV24_709_ER;
    if (!strcmp(userdefined_fmt, "NV24_10LE_709"))
        return NvBufferColorFormat_NV24_10LE_709;
    if (!strcmp(userdefined_fmt, "NV24_10LE_709_ER"))
        return NvBufferColorFormat_NV24_10LE_709_ER;
    if (!strcmp(userdefined_fmt, "NV24_10LE_2020"))
        return NvBufferColorFormat_NV24_10LE_2020;
    if (!strcmp(userdefined_fmt, "NV24_12LE_2020"))
        return NvBufferColorFormat_NV24_12LE_2020;
    return NvBufferColorFormat_Invalid;
}

int
parse_csv_args(context_t * ctx, int argc, char *argv[])
{
    char **argp = argv;
    char *arg = *(++argp);

    if (argc == 1 || (arg && (!strcmp(arg, "-h") || !strcmp(arg, "--help"))))
    {
        print_help();
        exit(EXIT_SUCCESS);
    }

    CSV_PARSE_CHECK_ERROR(argc < 9, "Insufficient arguments");

    ctx->in_file_path = strdup(*argp);
    CSV_PARSE_CHECK_ERROR(!ctx->in_file_path, "Input file not specified");

    ctx->in_width = atoi(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->in_width == 0, "Input width should be > 0");

    ctx->in_height = atoi(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->in_height == 0, "Input height should be > 0");

    ctx->in_pixfmt = get_color_format(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->in_pixfmt == NvBufferColorFormat_Invalid, "Incorrect input format");

    ctx->out_file_path = strdup(*(++argp));
    CSV_PARSE_CHECK_ERROR(!ctx->out_file_path, "Output file not specified");

    ctx->out_width = atoi(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->out_width == 0, "Output width should be > 0");

    ctx->out_height = atoi(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->out_height == 0, "Output height should be > 0");

    ctx->out_pixfmt = get_color_format(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->out_pixfmt == NvBufferColorFormat_Invalid, "Incorrect output format");

    while ((arg = *(++argp)))
    {
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
            print_help();
            exit(EXIT_SUCCESS);
        }
        else if (!strcmp(arg, "-t") || !strcmp(arg, "--num-thread"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->num_thread = atoi(*argp);
        }
        else if (!strcmp(arg, "-a") || !strcmp(arg, "--async"))
        {
            ctx->async = true;
        }
        else if (!strcmp(arg, "-s") || !strcmp(arg, "--create-session"))
        {
            ctx->create_session = true;
        }
        else if (!strcmp(arg, "-p") || !strcmp(arg, "--perf"))
        {
            ctx->perf = true;
        }
        else if (!strcmp(arg, "-fm"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->flip_method = (NvBufferTransform_Flip) atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->flip_method < NvBufferTransform_None ||
                     ctx->flip_method > NvBufferTransform_InvTranspose),
                    "Unsupported value for flip-method: " << *argp);
        }
        else if (!strcmp(arg, "-im"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->interpolation_method =
                (NvBufferTransform_Filter) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->interpolation_method < NvBufferTransform_Filter_Nearest ||
                     ctx->interpolation_method > NvBufferTransform_Filter_Nicest),
                    "Unsupported value for interpolation-method: " << *argp);
        }
        else if (!strcmp(arg, "-cr"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->crop_rect.left = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->crop_rect.left < 0 ||
                    ctx->crop_rect.left >= ctx->in_width),
                    "Crop left param out of bounds");

            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->crop_rect.top = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->crop_rect.top < 0 ||
                    ctx->crop_rect.top >= ctx->in_height),
                    "Crop top param out of bounds");

            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->crop_rect.width = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->crop_rect.width <= 0 ||
                    ctx->crop_rect.left + ctx->crop_rect.width >
                    ctx->in_width),
                    "Crop width param out of bounds");

            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->crop_rect.height = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->crop_rect.height <= 0 ||
                    ctx->crop_rect.top + ctx->crop_rect.height >
                    ctx->in_height),
                    "Crop height param out of bounds");
        }
        else
        {
            CSV_PARSE_CHECK_ERROR(ctx->out_file_path, "Unknown option " << arg);
        }
    }

    return 0;

error:
    print_help();
    return -1;
}
