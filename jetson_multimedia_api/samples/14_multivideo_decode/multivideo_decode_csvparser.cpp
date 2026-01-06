/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
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

#include "multivideo_decode.h"


using namespace std;

#define CHECK_OPTION_VALUE(argp) if(!*argp || (*argp)[0] == '-') \
                                { \
                                    cerr << "Error: value not specified for option " << arg << endl; \
                                    goto error; \
                                }

#define CHECK_IF_LAST_LOOP(i, num_files, argp, dec) if ( i + 1 < num_files ) \
                                { \
                                    argp-=dec; \
                                }

#define CSV_PARSE_CHECK_ERROR(condition, str) \
    if (condition) {\
    cerr << "Error: " << str << endl; \
    goto error; \
    }


static void
print_help(void)
{
    cerr << "\nmultivideo_decode num_files <number_of_files> <file_name1> <in-format1> -o <out_filename1> "
            "<file_name2> <in-format2> -o <out_filename2> --disable-rendering [options] \n\n"
            "Supported formats:\n"
            "\tVP9\n"
            "\tVP8\n"
            "\tH264\n"
            "\tH265\n"
            "\tMPEG2\n"
            "\tMPEG4\n\n"
            "OPTIONS:\n"
            "\tNOTE: Currently multivideo_decode to be only run with --disable-rendering Mandatory\n"
            "\t-h,--help            Prints this text\n"
            "\t--dbg-level <level>  Sets the debug level [Values 0-3]\n\n"
            "\t--stats              Report profiling data for the app\n\n"
            "\tNOTE: this should not be used alongside -o option as it decreases the FPS value shown in --stats\n"
            "\t--disable-rendering  Disable rendering\n"
            "\tNOTE: this should be set only for platform T194 or above\n"
            "\t--fullscreen         Fullscreen playback [Default = disabled]\n"
            "\t-ww <width>          Window width in pixels [Default = video-width]\n"
            "\t-wh <height>         Window height in pixels [Default = video-height]\n"
            "\t-loop <count>        Playback in a loop.[count = 1,2,...,n times looping , 0 = infinite looping]\n"
            "\tNOTE: -queue should be the last option mentioned in the command line no other option should be mentioned after that.\n"
            "\t-wx <x-offset>       Horizontal window offset [Default = 0]\n"
            "\t-wy <y-offset>       Vertical window offset [Default = 0]\n\n"
            "\t-fps <fps>           Display rate in frames per second [Default = 30]\n\n"
            "\t-o <out-file>        Write to output file\n\n"
            "\tNOTE: Not to be used along-side -loop and -queue option.\n"
            "\t-f <out_pixfmt>      1 NV12, 2 I420 [Default = 1]\n\n"
            "\t-sf <value>          Skip frames while decoding [Default = 0]\n"
            "\tAllowed values for the skip-frames parameter:\n"
            "\t0 = Decode all frames\n"
            "\t1 = Skip non-reference frames\n"
            "\t2 = Decode only key frames\n\n"
            "\t--input-nalu         Input to the decoder will be nal units\n"
            "\t--input-chunks       Input to the decoder will be a chunk of bytes [Default]\n\n"
            "\t--copy-timestamp <st> <fps> Enable copy timestamp with start timestamp(st) in seconds for decode fps(fps) (for input-nalu mode)\n"
            "\tNOTE: copy-timestamp used to demonstrate how timestamp can be associated with an individual H264/H265 frame to achieve video-synchronization.\n"
            "\t      currenly only supported for H264 & H265 video encode using MM APIs and is only for demonstration purpose.\n"
            "\t--report-metadata    Enable metadata reporting\n\n"
            "\t--blocking-mode <val> Set blocking mode, 0 is non-blocking, 1 for blocking (Default) \n\n"
            "\t--report-input-metadata  Enable metadata reporting for input header parsing error\n\n"
            "\t-v4l2-memory-out-plane <num>       Specify memory type to be used on Output Plane [1 = V4L2_MEMORY_MMAP, 2 = V4L2_MEMORY_USERPTR], Default = V4L2_MEMORY_MMAP\n\n"
            "\t-v4l2-memory-cap-plane <num>       Specify memory type to be used on Capture Plane [1 = V4L2_MEMORY_MMAP, 2 = V4L2_MEMORY_DMABUF], Default = V4L2_MEMORY_DMABUF\n\n"
            "\t-s <loop-count>      Stress test [Default = 1]\n\n"
#ifndef USE_NVBUF_TRANSFORM_API
            "\t--do-yuv-rescale     Rescale decoded YUV from full range to limited range\n\n"
#endif
            ;
}

static uint32_t
get_decoder_type(char *arg)
{
    if (!strcmp(arg, "H264"))
        return V4L2_PIX_FMT_H264;
    if (!strcmp(arg, "H265"))
        return V4L2_PIX_FMT_H265;
    if (!strcmp(arg, "VP9"))
        return V4L2_PIX_FMT_VP9;
    if (!strcmp(arg, "VP8"))
        return V4L2_PIX_FMT_VP8;
    if (!strcmp(arg, "MPEG2"))
        return V4L2_PIX_FMT_MPEG2;
    if (!strcmp(arg, "MPEG4"))
        return V4L2_PIX_FMT_MPEG4;
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

    if(!strcmp(arg, "num_files"))
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

    for ( int i = 0 ; i < num_files ; i++ )
    {

        CHECK_OPTION_VALUE(argp);
        ctx[i]->in_file_path = strdup(*argp);
        CSV_PARSE_CHECK_ERROR(!ctx[i]->in_file_path,
                              "Input file not specified");

        argp++;
        argc--;
        ctx[i]->decoder_pixfmt = get_decoder_type(*(argp));
        CSV_PARSE_CHECK_ERROR(ctx[i]->decoder_pixfmt == 0,
                          "Incorrect input format");

        arg = *(++argp);
        argc--;
        CSV_PARSE_CHECK_ERROR((argc == 0),
                                  "--disable-rendering not specified");
        if (!strcmp(arg, "-o"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx[i]->out_file_path = strdup(*argp);
            CSV_PARSE_CHECK_ERROR(!ctx[i]->out_file_path,
                                  "Output file not specified");
        }
        else
        {
            argp--;
            argc++;
        }
        argp++;
        argc--;
    }

    argp--;
    while ((arg = *(++argp)))
    {
        int i;
        for ( i = 0 ; i < num_files ; i++ )
        {    if (!strcmp(arg, "-f"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->out_pixfmt = atoi(*argp);
                CSV_PARSE_CHECK_ERROR((ctx[i]->out_pixfmt < 1 || ctx[i]->out_pixfmt > 2),
                                        "format shoud be 1(NV12), 2(I420)");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "--stats"))
            {
                ctx[i]->stats = true;
            }
            else if (!strcmp(arg, "--disable-rendering"))
            {
                ctx[i]->disable_rendering = true;
            }
            else if (!strcmp(arg, "--disable-dpb"))
            {
                ctx[i]->disable_dpb = true;
            }
            else if (!strcmp(arg, "--fullscreen"))
            {
                ctx[i]->fullscreen = true;
            }
            else if (!strcmp(arg, "-wh"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->window_height = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->window_height == 0,
                                      "Window height should be > 0");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-ww"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->window_width = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->window_width == 0,
                                      "Window width should be > 0");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-wx"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->window_x = atoi(*argp);
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-wy"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->window_y = atoi(*argp);
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-fps"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->fps = atof(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->fps == 0, "FPS should be > 0");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "--input-nalu"))
            {
                CSV_PARSE_CHECK_ERROR(ctx[i]->decoder_pixfmt == V4L2_PIX_FMT_VP8, "VP8 does not support --input-nalu");
                CSV_PARSE_CHECK_ERROR(ctx[i]->decoder_pixfmt == V4L2_PIX_FMT_VP9, "VP9 does not support --input-nalu");
                ctx[i]->input_nalu = true;
            }
            else if (!strcmp(arg, "--input-chunks"))
            {
                ctx[i]->input_nalu = false;
            }
            else if (!strcmp(arg, "--copy-timestamp"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->start_ts = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->start_ts < 0, "start timestamp should be >= 0");
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->dec_fps = atof(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->dec_fps <= 0, "decode fps should be > 0");
                ctx[i]->copy_timestamp = true;
                CHECK_IF_LAST_LOOP(i, num_files, argp, 2);
            }
            else if (!strcmp(arg, "--report-metadata"))
            {
                ctx[i]->enable_metadata = true;
            }
            else if (!strcmp(arg, "--report-input-metadata"))
            {
                ctx[i]->enable_input_metadata = true;
            }
            else if (!strcmp(arg, "-s"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->stress_test = atoi(*argp);
                CSV_PARSE_CHECK_ERROR(ctx[i]->stress_test <= 0,
                        "stress times should be bigger than 0");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if(!strcmp(arg, "-v4l2-memory-out-plane"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->output_plane_mem_type = (enum v4l2_memory) atoi(*argp);
                CSV_PARSE_CHECK_ERROR(
                        (ctx[i]->output_plane_mem_type > V4L2_MEMORY_USERPTR ||
                         ctx[i]->output_plane_mem_type < V4L2_MEMORY_MMAP),
                        "Unsupported v4l2 memory type: " << *argp);
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-sf"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->skip_frames = (enum v4l2_skip_frames_type) atoi(*argp);
                CSV_PARSE_CHECK_ERROR(
                        (ctx[i]->skip_frames > V4L2_SKIP_FRAMES_TYPE_DECODE_IDR_ONLY ||
                         ctx[i]->skip_frames < V4L2_SKIP_FRAMES_TYPE_NONE),
                        "Unsupported values for skip frames: " << *argp);
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "--dbg-level"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                log_level = get_dbg_level(*argp);
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
            {
                print_help();
                exit(EXIT_SUCCESS);
            }
            else if (!strcmp(arg, "-v4l2-memory-cap-plane"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                int num = (uint32_t) atoi(*argp);
                switch(num)
                {
                    case 1  :ctx[i]->capture_plane_mem_type = V4L2_MEMORY_MMAP;
                             break;
                    case 2  :ctx[i]->capture_plane_mem_type = V4L2_MEMORY_DMABUF;
                             break;
                }
                CSV_PARSE_CHECK_ERROR(!(num > 0 && num < 3),
                        "Memory type selection should be > 0 and < 3");
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
            else if (!strcmp(arg, "--blocking-mode"))
            {
                argp++;
                CHECK_OPTION_VALUE(argp);
                ctx[i]->blocking_mode = atoi(*argp);
                CHECK_IF_LAST_LOOP(i, num_files, argp, 1);
            }
#ifndef USE_NVBUF_TRANSFORM_API
            else if (!strcmp(arg, "--do-yuv-rescale"))
            {
                ctx[i]->rescale_method = V4L2_YUV_RESCALE_EXT_TO_STD;
            }
#endif
            else
            {
                CSV_PARSE_CHECK_ERROR(ctx[i]->in_file_path, "Unknown option " << arg);
            }
        }
    }

    return 0;

error:
    print_help();
    return -1;
}
