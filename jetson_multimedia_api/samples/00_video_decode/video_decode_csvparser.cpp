/*
 * Copyright (c) 2016-2024, NVIDIA CORPORATION. All rights reserved.
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

#include "video_decode.h"

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
    cerr << "\nvideo_decode <in-format> [options] <in-file>\n\n"
            "Supported formats:\n"
            "\tVP9\n"
            "\tVP8\n"
            "\tH264\n"
            "\tH265\n"
            "\tMPEG2\n"
            "\tMPEG4\n"
            "\tMJPEG\n\n"
            "OPTIONS:\n"
            "\t-h,--help            Prints this text\n"
            "\t--dbg-level <level>  Sets the debug level [Values 0-3]\n\n"
            "\t--stats              Report profiling data for the app\n\n"
            "\tNOTE: this should not be used alongside -o option as it decreases the FPS value shown in --stats\n"
            "\t--disable-rendering  Disable rendering\n"
            "\t--max-perf           Enable maximum Performance \n"
            "\tNOTE: this should be set only for platform T194 or above\n"
            "\t--fullscreen         Fullscreen playback [Default = disabled]\n"
            "\t-ww <width>          Window width in pixels [Default = video-width]\n"
            "\t-wh <height>         Window height in pixels [Default = video-height]\n"
            "\t-loop <count>        Playback in a loop.[count = 1,2,...,n times looping , 0 = infinite looping]\n"
            "\t-queue [<file1> <file2> ....] Files are played in a queue manner\n"
            "\tNOTE: -queue should be the last option mentioned in the command line no other option should be mentioned after that.\n"
            "\t-wx <x-offset>       Horizontal window offset [Default = 0]\n"
            "\t-wy <y-offset>       Vertical window offset [Default = 0]\n\n"
            "\t-fps <fps>           Display rate in frames per second [Default = 30]\n\n"
            "\t-o <out-file>        Write to output file\n\n"
            "\tNOTE: Not to be used along-side -loop and -queue option.\n"
            "\t-f <out_pixfmt>      1 NV12, 2 I420, 3 NV16, 4 NV24, 5 I422, 6 I444 7 YUYV [Default = 1]\n"
            "\tNOTE: -f <5/6/7> is supported only for MJPEG format.\n\n"
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
            "\t-extra_cap_plane_buffer <num>      Specify extra capture plane buffers (Default=1, MAX=32) to be allocated\n"
#ifndef USE_NVBUF_TRANSFORM_API
            "\t--do-yuv-rescale     Rescale decoded YUV from full range to limited range\n\n"
#endif
            "\t-chroma-loc-horiz <val>    Sets the horizontal chroma location [Values 0-5]\n"
            "\t-chroma-loc-vert <val>     Sets the vertical chroma location [Values 0-5]\n"
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
    if (!strcmp(arg, "MJPEG"))
        return V4L2_PIX_FMT_MJPEG;
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

    CSV_PARSE_CHECK_ERROR(argc < 3, "Insufficient arguments");

    ctx->decoder_pixfmt = get_decoder_type(*(argp));
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
            CSV_PARSE_CHECK_ERROR((ctx->out_pixfmt < 1 || ctx->out_pixfmt > 7),
                                    "format should be 1(NV12), 2(I420), 3(NV16), 4(NV24) 5(I422), 6(I444), 7 (YUYV)");
        }
        else if (!strcmp(arg, "--stats"))
        {
            ctx->stats = true;
        }
        else if (!strcmp(arg, "--disable-rendering"))
        {
            ctx->disable_rendering = true;
        }
        else if (!strcmp(arg, "--disable-dpb"))
        {
            ctx->disable_dpb = true;
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
        else if (!strcmp(arg, "--copy-timestamp"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->start_ts = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->start_ts < 0, "start timestamp should be >= 0");
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->dec_fps = atof(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->dec_fps <= 0, "decode fps should be > 0");
            ctx->copy_timestamp = true;
        }
        else if (!strcmp(arg, "--report-metadata"))
        {
            ctx->enable_metadata = true;
        }
        else if (!strcmp(arg, "--report-input-metadata"))
        {
            ctx->enable_input_metadata = true;
        }
        else if (!strcmp(arg, "-s"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->stress_test = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->stress_test <= 0,
                    "stress times should be bigger than 0");
        }
        else if(!strcmp(arg, "-v4l2-memory-out-plane"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->output_plane_mem_type = (enum v4l2_memory) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->output_plane_mem_type > V4L2_MEMORY_USERPTR ||
                     ctx->output_plane_mem_type < V4L2_MEMORY_MMAP),
                    "Unsupported v4l2 memory type: " << *argp);
        }
        else if(!strcmp(arg, "--max-perf"))
        {
            ctx->max_perf = 1;
        }
        else if(!strcmp(arg, "-extra_cap_plane_buffer"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->extra_cap_plane_buffer = atoi(*argp);;
            CSV_PARSE_CHECK_ERROR((ctx->extra_cap_plane_buffer < 1 || ctx->extra_cap_plane_buffer > 32),
                                    "extra capture plane buffer should be greater than 0 & less than 33");
        }
        else if (!strcmp(arg, "-sf"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->skip_frames = (enum v4l2_skip_frames_type) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->skip_frames > V4L2_SKIP_FRAMES_TYPE_DECODE_IDR_ONLY ||
                     ctx->skip_frames < V4L2_SKIP_FRAMES_TYPE_NONE),
                    "Unsupported values for skip frames: " << *argp);
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
        else if (!strcmp(arg, "-loop"))
        {
            ctx->bLoop = true;
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->loop_count = atoi(*argp);
        }
        else if (!strcmp(arg, "-queue"))
        {
            ctx->bQueue = true;
            break;
        }
        else if (!strcmp(arg, "-v4l2-memory-cap-plane"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            int num = (uint32_t) atoi(*argp);
            switch(num)
            {
                case 1  :ctx->capture_plane_mem_type = V4L2_MEMORY_MMAP;
                         break;
                case 2  :ctx->capture_plane_mem_type = V4L2_MEMORY_DMABUF;
                         break;
            }
            CSV_PARSE_CHECK_ERROR(!(num > 0 && num < 3),
                    "Memory type selection should be > 0 and < 3");
        }
        else if (!strcmp(arg, "--blocking-mode"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->blocking_mode = atoi(*argp);
        }
#ifndef USE_NVBUF_TRANSFORM_API
        else if (!strcmp(arg, "--do-yuv-rescale"))
        {
            ctx->rescale_method = V4L2_YUV_RESCALE_EXT_TO_STD;
        }
#endif
        else if (!strcmp(arg, "-chroma-loc-horiz"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->chromaLocHoriz = atoi(*argp);
        }
        else if (!strcmp(arg, "-chroma-loc-vert"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->chromaLocVert = atoi(*argp);
        }
        else
        {
            CSV_PARSE_CHECK_ERROR(ctx->in_file_path, "Unknown option " << arg);
        }
    }

    if(ctx->bQueue)
    {
        ctx->file_count=0;

        while((arg = *(++argp)))
        {
            ctx->file_count++;
        }
        int i = ctx->file_count+1;
        while(i--)
        {
            --argp;
        }

        ctx->in_file_path = (char **)malloc(sizeof(char *)*ctx->file_count);
        i=0;
        while((arg = *(++argp)))
        {
            ctx->in_file_path[i++] = strdup(*argp);
            CSV_PARSE_CHECK_ERROR(!ctx->in_file_path, "Input file not specified");
        }
    }
    else
    {
        ctx->in_file_path = (char **)malloc(sizeof(char *)*1);
        ctx->in_file_path[0] = strdup(*--argp);
        CSV_PARSE_CHECK_ERROR(!ctx->in_file_path, "Input file not specified");
    }
    return 0;

error:
    print_help();
    return -1;
}
