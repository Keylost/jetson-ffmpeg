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
#ifdef ENABLE_TRT
#include "trt_inference.h"
#endif
#include "v4l2_backend_test.h"

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

void
print_help(void)
{
    cerr << "\nbackend <channel-num> <in-file1> <in-file2>... <in-format> [options]\n\n"
            "Channel-num:\n"
            "\t1-4, Number of file arguments should exactly match the number of channels specified\n\n"
            "Supported formats:\n"
            "\tH264\n"
            "\tH265\n\n"
            "OPTIONS:\n"
            "\t-h,--help            Prints this text\n"
            "\t-fps <fps>           Display rate in frames per second [Default = 30]\n\n"
            "\t--s                  Give a statistic of each channel\n"
            "\t--input-nalu         Input to the decoder will be nal units[Default]\n"
            "\t--input-chunks       Input to the decoder will be a chunk of bytes\n\n"
#ifdef ENABLE_TRT
            "\t--trt-deployfile     set deploy file name\n"
            "\t--trt-modelfile      set model file name\n"
            "\t--trt-proc-interval  set process interval, 1 frame will be process every trt-proc-interval\n"
            "\t--trt-mode           0 fp16 (if supported), 1 fp32, 2 int8\n"
            "\t--trt-dumpresult     1 to dump result, 0[default] otherwise\n"
            "\t--trt-enable-perf    1[default] to enable perf measurement, 0 otherwise\n"
#else
            "\t-run-opt <0-3>       0[default], 1 parser only, 2 parser+decoder,  3 parser+decoder+VIC\n"
#endif
            << endl;
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

int
parse_csv_args(context_t * ctx,
#ifdef ENABLE_TRT
            TRT_Context *trt_ctx,
#endif
            int argc, char *argv[])
{
    char **argp = argv;
    char *arg = *(++argp);

    ctx->decoder_pixfmt = get_decoder_type(*argp);
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
        else if (!strcmp(arg, "--s"))
        {
            ctx->do_stat = true;
        }
        else if (!strcmp(arg, "--input-nalu"))
        {
            ctx->input_nalu = true;
        }
        else if (!strcmp(arg, "--input-chunks"))
        {
            ctx->input_nalu = false;
        }
        else if (!strcmp(arg, "--dbg-level"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            log_level = atoi(*argp);
        }
        else if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
            print_help();
            exit(EXIT_SUCCESS);
        }
#ifdef ENABLE_TRT
        else if (!strcmp(arg, "--trt-deployfile"))
        {
            argp++;
            /* This parameter has been parsed in global_cfg,
               but need to skip if found here */
            continue;
        }
        else if (!strcmp(arg, "--trt-modelfile"))
        {
            argp++;
            /* This parameter has been parsed in global_cfg,
               but need to skip if found here */
            continue;
        }
        else if (!strcmp(arg, "--trt-mode"))
        {
            argp++;
            trt_ctx->setMode(atoi(*argp));
        }
        else if (!strcmp(arg, "--trt-proc-interval"))
        {
            argp++;
            trt_ctx->setFilterNum(atoi(*argp));
        }
        else if (!strcmp(arg, "--trt-dumpresult"))
        {
            if (*(argp + 1) != NULL &&
                (strcmp(*(argp + 1), "0") == 0 ||
                strcmp(*(argp + 1), "1") == 0))
            {
                argp++;
                trt_ctx->setDumpResult((bool)atoi(*argp));
            }
        }
        else if (!strcmp(arg, "--trt-enable-perf"))
        {
            if (*(argp + 1) != NULL &&
                (strcmp(*(argp + 1), "0") == 0 ||
                strcmp(*(argp + 1), "1") == 0))
            {
                argp++;
                trt_ctx->setTrtProfilerEnabled((bool)atoi(*argp));
            }
        }
#else
        else if (!strcmp(arg, "-run-opt"))
        {
            argp++;
            ctx->cpu_occupation_option = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->cpu_occupation_option < 0 ||
                                  ctx->cpu_occupation_option >3,
                                  "parameter error:run-opt should be 0-3");
        }
#endif
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

void
parse_global(global_cfg* cfg, int argc, char ***argv)
{
    char **argp = *argv;
    char *arg = *(++argp);
    if (argc == 1 || (arg && (!strcmp(arg, "-h") || !strcmp(arg, "--help"))))
    {
        print_help();
        exit(1);
    }
    cfg->channel_num = atoi(*argp);
    CSV_PARSE_CHECK_ERROR(cfg->channel_num < 1 ||  cfg->channel_num > 4,
                    "channel should be between 1 and 4, program will exit");

    for (uint32_t i = 0; i < cfg->channel_num; i++)
    {
        if (*(argp + 1) != NULL && strcmp(*(argp + 1), "H264") != 0 &&
            strcmp(*(argp + 1), "H265") != 0)
        {
            cfg->in_file_path[i] = *(++argp);
        }
        else
        {
            cout << "Not enough number of files provided" << endl;
            print_help();
            exit(1);
        }
    }

    *argv = argp;

#ifdef ENABLE_TRT
    // seek for TRT deploy & caffemodel
    while ((arg = *(++argp)))
    {
        if (!strcmp(arg, "--trt-deployfile"))
        {
            argp++;
            cfg->deployfile = *argp;
        }
        else if (!strcmp(arg, "--trt-modelfile"))
        {
            argp++;
            cfg->modelfile = *argp;
        }
    }
#endif
    return;

error:
    exit(EXIT_SUCCESS);
}
