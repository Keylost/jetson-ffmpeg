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

#include "trt_inference.h"
#include "video_dec_trt.h"

#define CHECK_OPTION_VALUE(argp) \
            if (!*argp || (*argp)[0] == '-') \
            { \
                cerr << "Error: value not specified for option " << arg << endl; \
                goto error; \
            }

#define CSV_PARSE_CHECK_ERROR(condition, str) \
            if (condition) { \
                cerr << "Error: " << str << endl; \
                goto error; \
            }

using namespace std;

static void
printHelp(void)
{
    cerr << "\nvideo_dec_trt [Channel-num] <in-file1> <in-file2> ... <in-format> [options]\n\n"
            "Channel-num:\n"
            "\t1-32, Number of file arguments should exactly match the number of channels specified\n\n"
            "Supported formats:\n"
            "\tH264\n"
            "\tH265\n\n"
            "OPTIONS:\n"
            "\t-h,--help            Prints this text\n"
            "\t--dbg-level <level>  Sets the debug level [Values 0-3]\n\n"
            "\t Caffe model:\n"
            "\t--trt-deployfile     set caffe deploy file name\n"
            "\t--trt-modelfile      set caffe model file name\n"
            "\t ONNX model:\n"
            "\t--trt-onnxmodel      set onnx model file name, only support dynamic batch(N=-1) onnx model\n"
            "\t--trt-mode           0 fp16 (if supported), 1 fp32, 2 int8\n"
            "\t--trt-enable-perf    1[default] to enable perf measurement, 0 otherwise\n";
}

static uint32_t
getDecoderType(char *arg)
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
parseCsvArgs(AppDecContext *ctx, AppTRTContext *trt_ctx_wrap,
                int argc, char *argv[])
{
    char **argp = argv;
    char *arg = *(++argp);
    uint32_t pixfmt;

    if (argc == 1 || (arg && (!strcmp(arg, "-h") || !strcmp(arg, "--help"))))
    {
        printHelp();
        exit(EXIT_SUCCESS);
    }

    CSV_PARSE_CHECK_ERROR(argc < 3, "Insufficient arguments");
    trt_ctx_wrap->dec_num = atoi(*argp);

    for (int i = 0; i < trt_ctx_wrap->dec_num; i++)
    {
        ctx[i].in_file_path = strdup(*(++argp));
        CSV_PARSE_CHECK_ERROR(!ctx[i].in_file_path,
                            "Input file not specified");
    }

    pixfmt = getDecoderType(*(++argp));
    for (int i = 0; i < trt_ctx_wrap->dec_num; i++)
    {
        ctx[i].decoder_pixfmt = pixfmt;
        CSV_PARSE_CHECK_ERROR(ctx[i].decoder_pixfmt == 0,
                          "Incorrect input format");
    }

    while ((arg = *(++argp)))
    {
        if (!strcmp(arg, "--dbg-level"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            log_level = get_dbg_level(*argp);
        }
        else if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
            printHelp();
            exit(EXIT_SUCCESS);
        }
        else if (!strcmp(arg, "--trt-deployfile"))
        {
            argp++;
            trt_ctx_wrap->deployfile = *argp;
            cout<< "set deployfile: " << *argp << endl;
        }
        else if (!strcmp(arg, "--trt-modelfile"))
        {
            argp++;
            trt_ctx_wrap->modelfile = *argp;
            cout<< "set modefile: " << *argp << endl;
        }
        else if (!strcmp(arg, "--trt-onnxmodel"))
        {
            argp++;
            trt_ctx_wrap->onnxmodelfile = *argp;
            cout<< "set onnx modefile: " << *argp << endl;
        }
        else if (!strcmp(arg, "--trt-mode"))
        {
            argp++;
            trt_ctx_wrap->trt_ctx->setMode(atoi(*argp));
        }
        else if (!strcmp(arg, "--trt-enable-perf"))
        {
            if (*(argp + 1) != NULL &&
                (strcmp(*(argp + 1), "0") == 0 ||
                strcmp(*(argp + 1), "1") == 0))
            {
                argp++;
                trt_ctx_wrap->trt_ctx->setTrtProfilerEnabled((bool)atoi(*argp));
            }
        }
        else
        {
            CSV_PARSE_CHECK_ERROR(1, "Unknown option " << arg);
        }
    }

    return 0;

error:
    printHelp();
    return -1;
}
