/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION. All rights reserved.
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

#include "jpeg_decode.h"

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
        "\njpeg-decode num_files <num_files> <in-file1> <out-file1> <in-file2> <out-file2> [OPTIONS]\n\n"
        "OPTIONS:\n"
        "\t-h,--help            Prints this text\n"
        "\t num_files           number of files to decode simultaneously\n\n"
        "\t--dbg-level <level>  Sets the debug level [Values 0-3]\n\n"
        "\t--perf               Benchmark decoder performance\n\n"
        "\t--decode-fd          Uses FD as output of decoder [DEFAULT]\n"
        "\t--decode-buffer      Uses buffer as output of decoder\n\n"
        "\t-s <loop-count>      Stress test [Default = 1]\n\n";
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

    int i = 0;
    ctx->num_files = 1;

    if (argc == 1 || (arg && (!strcmp(arg, "-h") || !strcmp(arg, "--help"))))
    {
        print_help();
        exit(EXIT_SUCCESS);
    }

    CSV_PARSE_CHECK_ERROR(argc < 3, "Insufficient arguments");

    if (!strcmp (*argp, "num_files"))
    {
        argp++;
        ctx->num_files = atoi (*argp++);
        CSV_PARSE_CHECK_ERROR(ctx->num_files > 10, "Max 10 files can be decoded for demonstration");
    }
    else
    {
        cerr<<"First argument should be num_files"<<endl;
        goto error;
    }

    for(i = 0; i < ctx->num_files; i++)
    {
        ctx->in_file_path[i] = strdup(*argp++);
        CSV_PARSE_CHECK_ERROR(!ctx->in_file_path[i], "Input file not specified");

        ctx->out_file_path[i] = strdup(*(argp++));
        CSV_PARSE_CHECK_ERROR(!ctx->out_file_path[i], "Output file not specified");
    }

    while ((arg = *(argp)))
    {
        if (!strcmp(arg, "--dbg-level"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            log_level = get_dbg_level(*argp);
        }
        else if (!strcmp(arg, "--perf"))
        {
            argp++;
            ctx->perf = true;
        }
        else if (!strcmp(arg, "--decode-fd"))
        {
            argp++;
            ctx->use_fd = true;
        }
        else if (!strcmp(arg, "--decode-buffer"))
        {
            argp++;
            ctx->use_fd = false;
        }
        else if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
            print_help();
            exit(EXIT_SUCCESS);
        }
        else if (!strcmp(arg, "-s"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->stress_test = atoi(*argp++);
            CSV_PARSE_CHECK_ERROR(ctx->stress_test <= 0,
                    "stress times should be bigger than 0");
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
