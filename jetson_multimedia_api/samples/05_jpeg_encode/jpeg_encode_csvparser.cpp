/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION. All rights reserved.
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

#include "jpeg_encode.h"

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
        "\njpeg-encode <in-file> <in-width> <in-height> <out-file> [OPTIONS]\n\n"
        "OPTIONS:\n"
        "\t-h,--help            Prints this text\n"
        "\t--dbg-level <level>  Sets the debug level [Values 0-3]\n\n"
        "\t--perf               Benchmark encoder performance\n\n"
        "\t--encode-fd          Uses FD as input to encoder [DEFAULT]\n"
        "\t--encode-buffer      Uses buffer as input to encoder\n\n"
        "\t-f <pixfmt>          Color format of input to encoder (works only for --encode-fd) [1=YUV420(Default), 2=NV12]\n\n"
        "\t-crop <left> <top> <width> <height>  Cropping rectangle for JPEG encoder\n\n"
        "\t-s <loop-count>      Stress test [Default = 1]\n\n"
        "\t-scale_encode <scale_width> <scale_height>  Scale encoding with given scaled width and height encoder\n\n"
        "\t-quality <value>     Sets the image quality [75(default)]\n\n";
}

static int32_t
get_dbg_level(char *arg)
{
    int32_t log_level = atoi(arg);
    if (log_level < 0)
    {
        cout<<"log level too small, set to 0"<<endl;
        return 0;
    }

    if (log_level > 3)
    {
        cout<<"log level too high, set to 3"<<endl;
        return 3;
    }

    return log_level;
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

    CSV_PARSE_CHECK_ERROR(argc < 5, "Insufficient arguments");

    ctx->in_file_path = strdup(*argp);
    CSV_PARSE_CHECK_ERROR(!ctx->in_file_path, "Input file not specified");

    ctx->in_width = atoi(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->in_width == 0, "Input width should be > 0");

    ctx->in_height = atoi(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->in_height == 0, "Input height should be > 0");

    ctx->out_file_path = strdup(*(++argp));
    CSV_PARSE_CHECK_ERROR(!ctx->out_file_path, "Output file not specified");

    while ((arg = *(++argp)))
    {
        if (!strcmp(arg, "--dbg-level"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            log_level = get_dbg_level(*argp);
        }
        else if (!strcmp(arg, "--perf"))
        {
            ctx->perf = true;
        }
        else if (!strcmp(arg, "--encode-fd"))
        {
            ctx->use_fd = true;
        }
        else if (!strcmp(arg, "--encode-buffer"))
        {
            ctx->use_fd = false;
        }
        else if (!strcmp(arg, "-f"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            switch(atoi(*argp))
            {
                case 1:
                    ctx->in_pixfmt = V4L2_PIX_FMT_YUV420M;
                    break;
                case 2:
                    ctx->in_pixfmt = V4L2_PIX_FMT_NV12M;
                    break;
                default:
                    CSV_PARSE_CHECK_ERROR(true, "Unsupported value for -f");
            }
        }
        else if (!strcmp(arg, "-s"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->stress_test = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->stress_test <= 0,
                    "stress times should be bigger than 0");
        }
        else if (!strcmp(arg, "-crop"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->crop_left = atoi(*argp);
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->crop_top = atoi(*argp);
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->crop_width = atoi(*argp);
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->crop_height = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->crop_width == 0 || ctx->crop_height == 0,
                    "Crop height/width should be greater than zero");
        }
        else if (!strcmp(arg, "-scale_encode"))
        {
            ctx->scaled_encode = TRUE;
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->scale_width = atoi(*argp);
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->scale_height = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->scale_width == 0 || ctx->scale_height == 0,
                    "scaled height/width should be greater than zero");
        }
        else if (!strcmp(arg, "-quality"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->quality = atoi(*argp);
        }
        else if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
            print_help();
            exit(EXIT_SUCCESS);
        }
        else
        {
            CSV_PARSE_CHECK_ERROR(ctx->out_file_path, "Unknown option " << arg);
        }
        CSV_PARSE_CHECK_ERROR(
                ctx->in_pixfmt == V4L2_PIX_FMT_NV12M && !ctx->use_fd,
                "--encode-buffer is not supported with NV12 format");
    }

    return 0;

error:
    print_help();
    return -1;
}
