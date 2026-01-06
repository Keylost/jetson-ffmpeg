/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
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
#include "video_cuda_enc.h"

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
    cerr << "\nvideo_cuda_enc <in-file> <in-width> <in-height> <encoder-type> <out-file> [OPTIONS]\n\n"
            "Encoder Types:\n"
            "\tH264\n"
            "\tH265\n\n"
            "OPTIONS:\n"
            "\t-h,--help             Prints this text\n"
            "\t--dbg-level <level>   Sets the debug level [Values 0-3]\n\n"
            "\t-br <bitrate>         Bitrate [Default = 4000000]\n"
            "\t-fps <num> <den>      Encoding fps in num/den [Default = 30/1]\n\n";
}

static int32_t
get_encoder_type(char *arg)
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
        else if (!strcmp(arg, "-br"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->bitrate = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->bitrate == 0, "bit rate should be > 0");
        }
        else
        {
            CSV_PARSE_CHECK_ERROR(ctx->out_file_path,
                        "Unknown option " << arg);
        }
    }

    return 0;

error:
    print_help();
    return -1;
}
