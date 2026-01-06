/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
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

#include "linux/videodev2.h"
#include "v4l2_nv_extensions.h"
#include <libv4l2.h>

using namespace std;

#include "camera_unit_sample.hpp"

#define CHECK_OPTION_VALUE(argp) \
    if(!*argp || (*argp)[0] == '-') \
   { \
       cerr << "Error: value not specified for option " << arg << endl; \
       goto error; \
   }

#define CSV_PARSE_CHECK_ERROR(condition, str) \
    if (condition) {\
    cerr << "Error: " << str << endl; \
    goto error; \
    }

static bool
isNumber(const char number[])
{
    int i = 0;

    // If a negative number
    if (number[0] == '-')
        i = 1;
    for (; number[i] != 0; i++)
    {
        if (!isdigit(number[i]))
        {
            if (number[i] != '.')
                return false;
        }
    }
    return true;
}

static void
print_help(void)
{
    cerr    << "\n./camera_sample [OPTIONS]\n\n"
            << "Argus Options:\n"
            << "\t-h, --help                Prints this text\n"
            << "\t-r <width> <height>       Camera capture resolution. [Default 1280 720]\n"
            << "\t-o <out-file>             Write raw buffers to output file\n"
            << "\t--mem-type <num>          Memory type to be used [1 = V4L2_MEMORY_MMAP, 3 = V4L2_MEMORY_DMABUF]\n"
            << "\t-fps <num> <den>          Capture frame rate. Capped by sensor mode selected. [Default = 30/1] \n"
            << "\t-sid <id>                 Sensor ID to be opened. [Default = 0]\n"
            << "\t-smode <mode>             Sensor mode to use. [Default = Selects the best match]\n"
            << "\t-awbm <enum>              Sets white balance which affects the picture color temperature.\n"
            << "\t                          [(1): off              - V4L2_ARGUS_AWB_MODE_OFF\n"
            << "\t                           (2): auto             - V4L2_ARGUS_AWB_MODE_AUTO\n"
            << "\t                           (3): incandescent     - V4L2_ARGUS_AWB_MODE_INCANDESCENT\n"
            << "\t                           (4): fluorescent      - V4L2_ARGUS_AWB_MODE_FLUORESCENT\n"
            << "\t                           (5): warm-fluorescent - V4L2_ARGUS_AWB_MODE_WARM_FLUORESCENT\n"
            << "\t                           (6): daylight         - V4L2_ARGUS_AWB_MODE_DAYLIGHT\n"
            << "\t                           (7): cloudy-daylight  - V4L2_ARGUS_AWB_MODE_CLOUDY_DAYLIGHT\n"
            << "\t                           (8): twilight         - V4L2_ARGUS_AWB_MODE_TWILIGHT\n"
            << "\t                           (9): shade            - V4L2_ARGUS_AWB_MODE_SHADE\n"
            << "\t                           (10): manual          - V4L2_ARGUS_AWB_MODE_MANUAL]\n"
            << "\t-tnrm <enum>              Sets temporal noise reduction mode.\n"
            << "\t                          [(1): Denoise_Off      - V4L2_ARGUS_DENOISE_MODE_OFF\n"
            << "\t                           (2): Denoise_Fast     - V4L2_ARGUS_DENOISE_MODE_FAST\n"
            << "\t                           (3): Denoise_HighQuality - V4L2_ARGUS_DENOISE_MODE_HIGH_QUALITY]\n"
            << "\t--tnrs <float>            Set temporal noise reduction strength. [Range: -1.0 1.0]\n"
            << "\t-eem <enum>               Set edge enhancement mode.\n"
            << "\t                          [(1): EdgeEnhancement_Off - V4L2_ARGUS_EDGE_ENHANCE_MODE_OFF\n"
            << "\t                           (2): EdgeEnhancement_Fast - V4L2_ARGUS_EDGE_ENHANCE_MODE_FAST\n"
            << "\t                           (3): EdgeEnhancement_HighQuality - V4L2_ARGUS_EDGE_ENHANCE_MODE_HIGH_QUALITY]\n"
            << "\t--ees <float>             Set edge enhancement strength. [Range: -1.0 1.0]\n"
            << "\t-aem <enum>               Set AC auto exposure antibanding mode.\n"
            << "\t                          [(1): AeAntibandingMode_Off  - V4L2_ARGUS_AE_ANTIBANDING_MODE_OFF\n"
            << "\t                           (2): AeAntibandingMode_Auto - V4L2_ARGUS_AE_ANTIBANDING_MODE_AUTO\n"
            << "\t                           (3): AeAntibandingMode_50HZ - V4L2_ARGUS_AE_ANTIBANDING_MODE_50HZ\n"
            << "\t                           (4): AeAntibandingMode_60HZ - V4L2_ARGUS_AE_ANTIBANDING_MODE_60HZ]\n"
            << "\t-lock <ae> <awb>          Set Auto Exposure and Auto White balance lock. [Default = 0 0]\n"
            << "\t--expc <float>            Adjust exposure compensation. [Range: -2.0 2.0]\n"
            << "\t-ispdr <float> <float>    Set ISP digital gain range. [Range: 1 256]\n"
            << "\t-gr <min> <max>           Set gain range. [Range: 1 16]\n"
            << "\t-expr <minexp> <maxexp>   Set exposure time range in nanoseconds. [Range: 34000 358733000]\n"
            << "\t-sat <color>              Adjust color saturation value. [Range: 0 2]\n"
            << "\t-nr                       Disable rendering.\n"
            << "\t-wres <wid> <ht>          Window display resolution.[Default: Frame resolution]\n"
            << "\t-woff <xoff> <yoff>       Window offset for {x,y}. [Default: 0 0]\n"
            << "\t-nf <num>                 Number of Frames. [Default = 300]\n"
            << "\t-s <loop_cnt>             Stress test count to run. [Default = 1]\n"
            << "\t-f                        Fullscreen Display mode. [Default = disabled]\n"
            << "\t--metadata                Get metadata by V4L2_CID_ARGUS_METADATA\n"
            << endl;
}

int
parse_cmdline_args(context_t * ctx, int argc, const char *argv[])
{
    const char **argp = argv;
    const char *arg = *(argp);

    CSV_PARSE_CHECK_ERROR(argc < 1, "Insufficient arguments");

    while ((arg = *(++argp)))
    {
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
            print_help();
            exit(EXIT_SUCCESS);
        }
        else if (!strcmp(arg, "-r"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->width = (uint32_t)atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->width == 0), "Input width should be > 0");
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->height = (uint32_t)atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->height == 0, "Input height should be > 0");
        }
        else if (!strcmp(arg, "-o"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->output_file_path = strdup(*argp);
            CSV_PARSE_CHECK_ERROR(!ctx->output_file_path,
                                  "Output file not specified");
        }
        else if (!strcmp(arg, "--mem-type"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            uint32_t num = (uint32_t)atoi(*argp);
            switch (num)
            {
                case 1:
                    ctx->capplane.mem_type = V4L2_MEMORY_MMAP;
                    break;
                case 3:
                    ctx->capplane.mem_type = V4L2_MEMORY_DMABUF;
                    break;
            }
            CSV_PARSE_CHECK_ERROR(!(num > 0 && num < 4), "Memory selection range:[1,3]");
        }
        else if (!strcmp(arg, "-fps"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->ctrls.fps_n = (uint32_t)atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->ctrls.fps_d < 0), "fps num should be > 0");
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->ctrls.fps_d = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->ctrls.fps_d <= 0), "fps den should be > 0");
        }
        else if (!strcmp(arg, "-sid"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->ctrls.sensor_id = (int32_t)atoi(*argp);
        }
        else if (!strcmp(arg, "-smode"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->ctrls.sensor_mode = atoi(*argp);
        }
        else if (!strcmp(arg, "-awbm"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->ctrls.autowhitebalance_mode =
                (enum v4l2_argus_ac_awb_mode) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->ctrls.autowhitebalance_mode <
                     V4L2_ARGUS_AWB_MODE_OFF ||
                     ctx->ctrls.autowhitebalance_mode >
                     V4L2_ARGUS_AWB_MODE_MANUAL),
                    "Unsupported value for AWB mode " << *argp);
            ctx->set_awbmode = true;
        }
        else if (!strcmp(arg, "-tnrm"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->ctrls.denoise_mode =
                (enum v4l2_argus_denoise_mode) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->ctrls.denoise_mode <
                     V4L2_ARGUS_DENOISE_MODE_OFF ||
                     ctx->ctrls.denoise_mode >
                     V4L2_ARGUS_DENOISE_MODE_HIGH_QUALITY),
                    "Unsupported value for tnr mode " << *argp);
            ctx->set_denoisemode = true;
        }
        else if (!strcmp(arg, "--tnrs"))
        {
            argp++;
            if (!*argp || (!isNumber(*argp)))
            {
                cerr << "Error: value not specified for option " << arg << endl; \
                goto error;
            }
            ctx->ctrls.denoise_strength = atof(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->ctrls.denoise_strength < -1.0f ||
                     ctx->ctrls.denoise_strength > 1.0f),
                    "Unsupported value for tnr strength " << *argp);
            ctx->set_denoise_strength = true;
        }
        else if (!strcmp(arg, "-eem"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->ctrls.edge_enhacement_mode =
                (enum v4l2_argus_edge_enhance_mode) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->ctrls.edge_enhacement_mode <
                     V4L2_ARGUS_EDGE_ENHANCE_MODE_OFF ||
                     ctx->ctrls.edge_enhacement_mode >
                     V4L2_ARGUS_EDGE_ENHANCE_MODE_HIGH_QUALITY),
                    "Unsupported value for ee mode " << *argp);
            ctx->set_eemode = true;
        }
        else if (!strcmp(arg, "--ees"))
        {
            argp++;
            if (!*argp || (!isNumber(*argp)))
            {
                cerr << "Error: value not specified for option " << arg << endl; \
                goto error;
            }
            ctx->ctrls.edge_enhacement_strength = atof(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->ctrls.edge_enhacement_strength < -1.0f ||
                     ctx->ctrls.edge_enhacement_strength > 1.0f),
                    "Unsupported value for ee strength " << *argp);
            ctx->set_eestrength = true;
        }
        else if (!strcmp(arg, "-aem"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->ctrls.ae_antibanding_mode =
                (enum v4l2_argus_ac_ae_antibanding_mode) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->ctrls.ae_antibanding_mode <
                     V4L2_ARGUS_AE_ANTIBANDING_MODE_OFF ||
                     ctx->ctrls.ae_antibanding_mode >
                     V4L2_ARGUS_AE_ANTIBANDING_MODE_60HZ),
                    "Unsupported value for ae antibanding mode " << *argp);
            ctx->set_aeantibandingmode = true;
        }
        else if (!strcmp(arg, "-lock"))
        {
            uint8_t lock = 0;
            argp++;
            CHECK_OPTION_VALUE(argp);
            lock = (uint8_t)atoi(*argp);
            CSV_PARSE_CHECK_ERROR((lock > 1), "Value should be either 0 or 1");
            ctx->ctrls.aelock = lock;
            argp++;
            CHECK_OPTION_VALUE(argp);
            lock = (uint8_t)atoi(*argp);
            CSV_PARSE_CHECK_ERROR((lock > 1), "Value should be either 0 or 1");
            ctx->ctrls.awblock = lock;
            ctx->set_autolock = true;
        }
        else if (!strcmp(arg, "--expc"))
        {
            argp++;
            if (!*argp || (!isNumber(*argp)))
            {
                cerr << "Error: value not specified for option " << arg << endl; \
                goto error;
            }
            ctx->ctrls.exposure_compensation = atof(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->ctrls.exposure_compensation < -2.0f ||
                     ctx->ctrls.exposure_compensation > 2.0f),
                    "Unsupported value for exposure compensation " << *argp);
            ctx->set_expcompensation = true;
        }
        else if (!strcmp(arg, "-ispdr"))
        {
            float val = 0.0;
            argp++;
            CHECK_OPTION_VALUE(argp);
            val = atof(*argp);
            CSV_PARSE_CHECK_ERROR((val < DEFAULT_ARGUS_DIGITAL_GAIN_RANGE_MIN),
                "ISP digital gain start value should be above min range");
            ctx->ctrls.isp_digital_gain_range.low = val;
            argp++;
            CHECK_OPTION_VALUE(argp);
            val = atof(*argp);
            CSV_PARSE_CHECK_ERROR((val > DEFAULT_ARGUS_DIGITAL_GAIN_RANGE_MAX),
                "ISP digital gain end value should be below max range");
            ctx->ctrls.isp_digital_gain_range.high = val;
            ctx->set_ispdigitalgainrange = true;
        }
        else if (!strcmp(arg, "-gr"))
        {
            float val = 0.0;
            argp++;
            CHECK_OPTION_VALUE(argp);
            val = atof(*argp);
            CSV_PARSE_CHECK_ERROR((val < DEFAULT_ARGUS_GAIN_RANGE_MIN),
                "Gain range start value should be above min range");
            ctx->ctrls.gain_range.low = val;
            argp++;
            CHECK_OPTION_VALUE(argp);
            val = atof(*argp);
            CSV_PARSE_CHECK_ERROR((val > DEFAULT_ARGUS_GAIN_RANGE_MAX),
                "Gain range end value should be below max range");
            ctx->ctrls.gain_range.high = val;
            ctx->set_gainrange = true;
        }
        else if (!strcmp(arg, "-expr"))
        {
            float val = 0.0;
            argp++;
            CHECK_OPTION_VALUE(argp);
            val = atof(*argp);
            CSV_PARSE_CHECK_ERROR((val < DEFAULT_ARGUS_EXPOSURE_TIME_MIN),
                "Exposure time range start value should be above min range");
            ctx->ctrls.exposure_time_range.low = val;
            argp++;
            CHECK_OPTION_VALUE(argp);
            val = atof(*argp);
            CSV_PARSE_CHECK_ERROR((val > DEFAULT_ARGUS_EXPOSURE_TIME_MAX),
                "Exposure time range end value should be below max range");
            ctx->ctrls.exposure_time_range.high = val;
            ctx->set_exptimerange = true;
        }
        else if (!strcmp(arg, "-sat"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->ctrls.color_saturation = atof(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->ctrls.color_saturation < 0.0f ||
                     ctx->ctrls.color_saturation > 2.0f),
                    "Unsupported value for color saturation " << *argp);
            ctx->set_colorsaturation = true;
        }
        else if (!strcmp(arg, "-nr"))
        {
            ctx->disable_rendering = true;
        }
        else if (!strcmp(arg, "-wres"))
        {
            int32_t valx = 0, valy = 0;
            argp++;
            CHECK_OPTION_VALUE(argp);
            valx = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((valx < 0),
                "Window width should be > 0");
            argp++;
            CHECK_OPTION_VALUE(argp);
            valy = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((valy < 0),
                "Window height should be > 0");
            ctx->display.window_width = valx;
            ctx->display.window_height = valy;
        }
        else if (!strcmp(arg, "-woff"))
        {
            int32_t valx = 0, valy = 0;
            argp++;
            CHECK_OPTION_VALUE(argp);
            valx = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((valx < 0),
                "Window x cord should be > 0");
            argp++;
            CHECK_OPTION_VALUE(argp);
            valy = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((valy < 0),
                "Window y cord should be > 0");
            ctx->display.window_xoff = valx;
            ctx->display.window_yoff = valy;
        }
        else if (!strcmp(arg, "-s"))
        {
            int32_t num = 0;
            argp++;
            CHECK_OPTION_VALUE(argp);
            num = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(num <= 0,
                    "stress times should be a non-negative value");
            ctx->stress_test = num;
        }
        else if (!strcmp(arg, "-f"))
        {
            ctx->fullscreen_mode = true;
        }
        else if (!strcmp(arg, "-nf"))
        {
            argp++;
            if (!argp)
            {
                cerr << "Error: value not specified for option " << arg << endl;
                goto error;
            }
            int num = (uint32_t)atoi(*argp);
            CSV_PARSE_CHECK_ERROR((num == 0), "Number of Frames should not be 0");
            ctx->num_frames = num;
        }
        else if (!strcmp(arg, "--metadata"))
        {
            ctx->enable_metadata = true;
        }
        else
        {
            cerr << "Unknown option " << endl;
            goto error;
        }
    }

    return 0;

error:
    print_help();
    return -1;
}
