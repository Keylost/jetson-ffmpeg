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
#include <cstring>
#include <libv4l2.h>
#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

using namespace std;

#include "camera_unit_sample_ctrls.hpp"

#define RETURN_ERROR_IF_FORMATS_NOT_SET() \
    if (capture_plane_pixfmt == 0) { \
        COMP_ERROR_MSG("Should be called after setting plane format") \
        return -1; \
    }

int set_framerate(int fd, uint32_t framerate_num, uint32_t framerate_den)
{
    struct v4l2_streamparm parms;
    int ret_val = 0;

    memset(&parms, 0, sizeof(parms));
    parms.parm.capture.timeperframe.numerator = framerate_den;
    parms.parm.capture.timeperframe.denominator = framerate_num;
    parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    ret_val = v4l2_ioctl(fd, VIDIOC_S_PARM, &parms);
    if (ret_val)
    {
        cerr << "Error while setting stream parameters" << endl;
    }
    return ret_val;
}

int set_extctrls(int fd, v4l2_ext_controls &ctrls)
{
    int ret_val = 0;

    ret_val = v4l2_ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);
    if (ret_val)
    {
      cerr << "Error while setting controls\n";
    }
    return ret_val;
}

int set_sensor_mode (int fd, int mode)
{
    struct v4l2_ext_control control;
    struct v4l2_ext_controls ctrls;

    memset (&control, 0, sizeof (control));
    memset (&ctrls, 0, sizeof (ctrls));

    ctrls.count = 1;
    ctrls.controls = &control;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

    control.id = V4L2_CID_ARGUS_SENSOR_MODE;
    control.value = mode;

    if (set_extctrls(fd, ctrls))
        return -1;

    cout << "Setting sensor mode to " << mode << endl;
    return 0;
}

int set_awb_mode (int fd, int mode)
{
    struct v4l2_ext_control control;
    struct v4l2_ext_controls ctrls;

    memset (&control, 0, sizeof (control));
    memset (&ctrls, 0, sizeof (ctrls));

    ctrls.count = 1;
    ctrls.controls = &control;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

    control.id = V4L2_CID_ARGUS_AUTO_WHITE_BALANCE_MODE;
    control.value = mode;

    if (set_extctrls(fd, ctrls))
        return -1;

    cout << "Setting AWB mode to " << mode << endl;
    return 0;
}

int set_denoise_mode (int fd, int mode)
{
    struct v4l2_ext_control control;
    struct v4l2_ext_controls ctrls;

    memset (&control, 0, sizeof (control));
    memset (&ctrls, 0, sizeof (ctrls));

    ctrls.count = 1;
    ctrls.controls = &control;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

    control.id = V4L2_CID_ARGUS_DENOISE_MODE;
    control.value = mode;

    if (set_extctrls(fd, ctrls))
        return -1;

    cout << "Setting TNR mode to " << mode << endl;
    return 0;
}

int set_ee_mode (int fd, int mode)
{
    struct v4l2_ext_control control;
    struct v4l2_ext_controls ctrls;

    memset (&control, 0, sizeof (control));
    memset (&ctrls, 0, sizeof (ctrls));

    ctrls.count = 1;
    ctrls.controls = &control;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

    control.id = V4L2_CID_ARGUS_EE_MODE;
    control.value = mode;

    if (set_extctrls(fd, ctrls))
        return -1;

    cout << "Setting EE mode to " << mode << endl;
    return 0;
}

int set_ae_antiband_mode (int fd, int mode)
{
    struct v4l2_ext_control control;
    struct v4l2_ext_controls ctrls;

    memset (&control, 0, sizeof (control));
    memset (&ctrls, 0, sizeof (ctrls));

    ctrls.count = 1;
    ctrls.controls = &control;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

    control.id = V4L2_CID_ARGUS_AE_ANTIBANDING_MODE;
    control.value = mode;

    if (set_extctrls(fd, ctrls))
        return -1;

    cout << "Setting AE antibanding mode to " << mode << endl;
    return 0;
}

int set_denoise_strength (int fd, float strength)
{
    struct v4l2_ext_control control;
    struct v4l2_ext_controls ctrls;
    v4l2_argus_denoise_strength denoise_str;

    memset (&control, 0, sizeof (control));
    memset (&ctrls, 0, sizeof (ctrls));

    denoise_str.DenoiseStrength = strength;

    ctrls.count = 1;
    ctrls.controls = &control;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

    control.id = V4L2_CID_ARGUS_DENOISE_STRENGTH;
    control.string = (char *)&denoise_str;

    if (set_extctrls(fd, ctrls))
        return -1;

    cout << "Setting Denoise strength to " << strength << endl;
    return 0;
}

int set_ee_strength (int fd, float strength)
{
    struct v4l2_ext_control control;
    struct v4l2_ext_controls ctrls;
    v4l2_argus_edge_enhance_strength ee_str;

    memset (&control, 0, sizeof (control));
    memset (&ctrls, 0, sizeof (ctrls));

    ee_str.EdgeEnhanceStrength = strength;

    ctrls.count = 1;
    ctrls.controls = &control;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

    control.id = V4L2_CID_ARGUS_EE_STRENGTH;
    control.string = (char *)&ee_str;

    if (set_extctrls(fd, ctrls))
        return -1;

    cout << "Setting EE strength to " << strength << endl;
    return 0;
}

int set_autolock (int fd, uint8_t aelock, uint8_t awblock)
{
    struct v4l2_ext_control control;
    struct v4l2_ext_controls ctrls;

    memset (&control, 0, sizeof (control));
    memset (&ctrls, 0, sizeof (ctrls));

    ctrls.count = 1;
    ctrls.controls = &control;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

    control.id = V4L2_CID_3A_LOCK;

    if (aelock)
    control.value |= V4L2_LOCK_EXPOSURE;

    if (awblock)
    control.value |= V4L2_LOCK_WHITE_BALANCE;

    if (set_extctrls(fd, ctrls))
        return -1;

    cout << "Setting AE lock " << (int)aelock
        << " AWB lock " << (int)awblock << endl;

    return 0;
}

int set_exposure_compensation (int fd, float compensation)
{
    struct v4l2_ext_control control;
    struct v4l2_ext_controls ctrls;
    v4l2_argus_exposure_compensation exp_comp;

    memset (&control, 0, sizeof (control));
    memset (&ctrls, 0, sizeof (ctrls));

    exp_comp.ExposureCompensation = compensation;

    ctrls.count = 1;
    ctrls.controls = &control;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

    control.id = V4L2_CID_ARGUS_EXPOSURE_COMPENSATION;
    control.string = (char *)&exp_comp;

    if (set_extctrls(fd, ctrls))
        return -1;

    cout << "Setting Exposure compensation " << compensation << endl;
    return 0;
}

int set_exp_time_range (int fd, float range_low, float range_high)
{
    struct v4l2_ext_control control;
    struct v4l2_ext_controls ctrls;
    v4l2_argus_exposure_timerange exposure;

    memset (&control, 0, sizeof (control));
    memset (&ctrls, 0, sizeof (ctrls));

    ctrls.count = 1;
    ctrls.controls = &control;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

    exposure.MinExposureTimeRange = range_low;
    exposure.MaxExposureTimeRange = range_high;
    control.id = V4L2_CID_ARGUS_EXPOSURE_TIME_RANGE;
    control.string = (char *)&exposure;

    if (set_extctrls(fd, ctrls))
        return -1;

    cout << "Setting Exposure time range " << (float)range_low << " "
    << (float)range_high << endl;
    return 0;
}

int set_gain_range (int fd, float range_low, float range_high)
{
    struct v4l2_ext_control control;
    struct v4l2_ext_controls ctrls;
    v4l2_argus_gainrange gain;

    memset (&control, 0, sizeof (control));
    memset (&ctrls, 0, sizeof (ctrls));

    ctrls.count = 1;
    ctrls.controls = &control;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

    gain.MinGainRange = range_low;
    gain.MaxGainRange = range_high;
    control.id = V4L2_CID_ARGUS_GAIN_RANGE;
    control.string = (char *)&gain;

    if (set_extctrls(fd, ctrls))
        return -1;

    cout << "Setting gain range " << range_low << " "
    << range_high << endl;
    return 0;
}

int set_digital_gain_range (int fd, float range_low, float range_high)
{
    struct v4l2_ext_control control;
    struct v4l2_ext_controls ctrls;
    v4l2_argus_ispdigital_gainrange digital_gain;

    memset (&control, 0, sizeof (control));
    memset (&ctrls, 0, sizeof (ctrls));

    ctrls.count = 1;
    ctrls.controls = &control;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

    digital_gain.MinISPDigitalGainRange = range_low;
    digital_gain.MaxISPDigitalGainRange = range_high;
    control.id = V4L2_CID_ARGUS_ISP_DIGITAL_GAIN_RANGE;
    control.string = (char *)&digital_gain;

    if (set_extctrls(fd, ctrls))
        return -1;

    cout << "Setting digital gain range " << range_low << " "
    << range_high << endl;
    return 0;
}

int set_color_saturation (int fd, float saturation)
{
    struct v4l2_ext_control control;
    struct v4l2_ext_controls ctrls;
    v4l2_argus_color_saturation color;

    memset (&control, 0, sizeof (control));
    memset (&ctrls, 0, sizeof (ctrls));

    color.EnableSaturation = true;
    color.ColorSaturation = saturation;

    ctrls.count = 1;
    ctrls.controls = &control;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

    control.id = V4L2_CID_ARGUS_COLOR_SATURATION;
    control.string = (char *)&color;

    if (set_extctrls(fd, ctrls))
        return -1;

    cout << "Setting color saturation " << saturation << endl;
    return 0;
}

int get_metadata(int fd, int index)
{
    struct v4l2_ext_controls extCtrls;
    struct v4l2_ext_control control;
    v4l2_argus_ctrl_metadata metadata;
    memset (&control, 0, sizeof (control));
    memset (&extCtrls, 0, sizeof (extCtrls));

    int ret_io;
    extCtrls.controls = &control;
    extCtrls.count = 1;
    extCtrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    metadata.BufferIndex = index;

    control.id = V4L2_CID_ARGUS_METADATA;
    control.string = (char *)&metadata;

    ret_io = v4l2_ioctl(fd, VIDIOC_G_EXT_CTRLS, &extCtrls);

    if (ret_io == 0)
    {
        fprintf(stderr,"\n %s Brightness %f AeLocked %d ValidFrameStatus %d" \
                "BufferIndex %d AwbCCT %u ISO %u FrameDuration %llu exposure %llu" \
                "IspDigitalGain %f SensorAnalogGain %f AEState %d AWBState %d\n", \
                strerror(errno), metadata.SceneLux, metadata.AeLocked, \
                metadata.ValidFrameStatus, metadata.BufferIndex, metadata.AwbCCT, \
                metadata.SensorSensitivity, metadata.FrameDuration, \
                metadata.SensorExposureTime, metadata.IspDigitalGain,\
                metadata.SensorAnalogGain, metadata.AEState, metadata.AWBState);
    }
    return ret_io;
}
