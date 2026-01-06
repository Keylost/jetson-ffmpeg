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

#include "videodec.h"

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
    cerr << "\nvideo_dec_cuda <in-file> <in-format> [options]\n\n"
            "Supported formats:\n"
            "\tH264\n"
            "\tH265\n\n"
            "OPTIONS:\n"
            "\t-h,--help            Prints this text\n"
            "\t--dbg-level <level>  Sets the debug level [Values 0-3]\n\n"
            "\t--disable-rendering  Disable rendering\n"
            "\t--fullscreen         Fullscreen playback [Default = disabled]\n"
            "\t-ww <width>          Window width in pixels [Default = video-width]\n"
            "\t-wh <height>         Window height in pixels [Default = video-height]\n"
            "\t-wx <x-offset>       Horizontal window offset [Default = 0]\n"
            "\t-wy <y-offset>       Vertical window offset [Default = 0]\n\n"
            "\t-fps <fps>           Display rate in frames per second [Default = 30]\n\n"
            "\t-o <out-file>        Write to output file\n\n"
            "\t-f <out_pixfmt>      1 NV12, 2 I420 [Default = 1]\n\n"
            "\t--input-nalu         Input to the decoder will be nal units\n"
            "\t--input-chunks       Input to the decoder will be a chunk of bytes [Default]\n"
            "\t--bbox-file          bbox file path\n"
            "\t--display-text <string>    enable nvosd text overlay with input string\n\n";
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

    CSV_PARSE_CHECK_ERROR(argc < 3, "Insufficient arguments");

    ctx->in_file_path = strdup(*argp);
    CSV_PARSE_CHECK_ERROR(!ctx->in_file_path, "Input file not specified");

    ctx->decoder_pixfmt = get_decoder_type(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->decoder_pixfmt == 0,
                          "Incorrect input format");

    while ((arg = *(++argp)))
    {
        if (!strcmp(arg, "-o"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->out_file_path = strdup(*argp);
            CSV_PARSE_CHECK_ERROR(!ctx->out_file_path,
                                  "Output file not specified");
        }
        else if (!strcmp(arg, "-f"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->out_pixfmt = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->out_pixfmt < 1 || ctx->out_pixfmt > 2),
                                    "out_pixfmt shoud be 1(NV12), 2(I420)");
        }
        else if (!strcmp(arg, "--disable-rendering"))
        {
            ctx->disable_rendering = true;
        }
        else if (!strcmp(arg, "--fullscreen"))
        {
            ctx->fullscreen = true;
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
        else if (!strcmp(arg, "--input-nalu"))
        {
            ctx->input_nalu = true;
        }
        else if (!strcmp(arg, "--input-chunks"))
        {
            ctx->input_nalu = false;
        }
        else if (!strcmp(arg, "--bbox-file"))
        {
            argp++;
            ctx->enable_osd = true;
            ctx->osd_file_path = strdup(*argp);
        }
        else if (!strcmp(arg, "--display-text"))
        {
            argp++;
            ctx->enable_osd_text = true;
            if (*argp)
                ctx->osd_text = strdup(*argp);
        }

        else if (!strcmp(arg, "--dbg-level"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            log_level = get_dbg_level(*argp);
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
    }

    return 0;

error:
    print_help();
    return -1;
}
