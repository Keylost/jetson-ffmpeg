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

#ifndef __CAMERA_SAMPLE__CTRLS_H__
#define __CAMERA_SAMPLE__CTRLS_H__

#include "linux/videodev2.h"
#include "v4l2_nv_extensions.h"
#include <libv4l2.h>

/**
 * @brief Sets the value of several controls.
 *
 * Calls \c VIDIOC_S_EXT_CTRLS IOCTL.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] ctrls A pointer to the controls to set.
 *
 * @return 0 for success, -1 otherwise.
 */
int set_extctrls(int fd, v4l2_ext_controls &ctrls);

/**
 * @brief Sets the argus camera capture framerate.
 *
 * Calls VIDIOC_S_PARM IOCTL on the capture plane.
 * Must be called after setting format on capture plane.
 * @note The framerate is capped by the sensor mode selected.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] framerate_num Numerator part of the framerate fraction.
 * @param[in] framerate_den Denominator part of the framerate fraction.
 *
 * @return 0 for success, -1 otherwise.
 */
int set_framerate(int fd, uint32_t framerate_num, uint32_t framerate_den);

/**
 * @brief Sets the argus camera sensor mode.
 *
 * Calls VIDIOC_S_EXT_CTRLS IOCTL internally with Control ID
 * \c V4L2_CID_ARGUS_SENSOR_MODE. Must be called after setting
 * format on capture plane.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] mode Sensor mode index to be selected.
 *
 * @return 0 for success, -1 otherwise.
 */
int set_sensor_mode (int fd, int mode);

/**
 * @brief Sets the argus camera white balance mode.
 *
 * Calls VIDIOC_S_EXT_CTRLS IOCTL internally with Control ID
 * \c V4L2_CID_ARGUS_AUTO_WHITE_BALANCE_MODE. Must be called
 * after setting format on capture plane.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] mode AWB mode to be selected.
 *
 * @return 0 for success, -1 otherwise.
 */
int set_awb_mode (int fd, int mode);

/**
 * @brief Sets the argus camera TNR mode.
 *
 * Calls VIDIOC_S_EXT_CTRLS IOCTL internally with Control ID
 * \c V4L2_CID_ARGUS_DENOISE_MODE. Must be called
 * after setting format on capture plane.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] mode TNR mode to be selected.
 *
 * @return 0 for success, -1 otherwise.
 */
int set_denoise_mode (int fd, int mode);

/**
 * @brief Sets the argus camera edge enhancement mode.
 *
 * Calls VIDIOC_S_EXT_CTRLS IOCTL internally with Control ID
 * \c V4L2_CID_ARGUS_EE_MODE. Must be called
 * after setting format on capture plane.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] mode EE mode to be selected.
 *
 * @return 0 for success, -1 otherwise.
 */
int set_ee_mode (int fd, int mode);

/**
 * @brief Sets the argus camera AE antibanding mode.
 *
 * Calls VIDIOC_S_EXT_CTRLS IOCTL internally with Control ID
 * \c V4L2_CID_ARGUS_AE_ANTIBANDING_MODE. Must be called
 * after setting format on capture plane.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] mode AE antibanding mode to be selected.
 *
 * @return 0 for success, -1 otherwise.
 */
int set_ae_antiband_mode (int fd, int mode);

/**
 * @brief Sets the argus camera TNR strength.
 *
 * Calls VIDIOC_S_EXT_CTRLS IOCTL internally with Control ID
 * \c V4L2_CID_ARGUS_DENOISE_STRENGTH. Must be called
 * after setting format on capture plane.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] strength TNR strength value.
 *
 * @return 0 for success, -1 otherwise.
 */
int set_denoise_strength (int fd, float strength);

/**
 * @brief Sets the argus camera edge enhancement strength.
 *
 * Calls VIDIOC_S_EXT_CTRLS IOCTL internally with Control ID
 * \c V4L2_CID_ARGUS_EE_STRENGTH. Must be called
 * after setting format on capture plane.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] strength EE strength value.
 *
 * @return 0 for success, -1 otherwise.
 */
int set_ee_strength (int fd, float strength);

/**
 * @brief Sets the argus camera auto exposure and auto white balance lock.
 *
 * Calls VIDIOC_S_EXT_CTRLS IOCTL internally with Control ID
 * \c V4L2_CID_3A_LOCK. Must be called
 * after setting format on capture plane.
 *
 * @param[in] fd Argusv4l2 context FD
 * @param[in] aelock lock/unlock boolean.
 * @param[in] awblock lock/unlock boolean.
 *
 * @return 0 for success, -1 otherwise.
 */
int set_autolock (int fd, uint8_t aelock, uint8_t awblock);

/**
 * @brief Sets the argus camera exposure compensation.
 *
 * Calls VIDIOC_S_EXT_CTRLS IOCTL internally with Control ID
 * \c V4L2_CID_ARGUS_EXPOSURE_COMPENSATION. Must be called
 * after setting format on capture plane.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] compensation Exposure value.
 *
 * @return 0 for success, -1 otherwise.
 */
int set_exposure_compensation (int fd, float compensation);

/**
 * @brief Sets the argus camera exposure time range.
 *
 * Calls VIDIOC_S_EXT_CTRLS IOCTL internally with Control ID
 * \c V4L2_CID_ARGUS_EXPOSURE_TIME_RANGE. Must be called
 * after setting format on capture plane.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] range_low Lower limit value.
 * @param[in] range_high Higher limit value.
 *
 * @return 0 for success, -1 otherwise.
 */
int set_exp_time_range (int fd, float range_low, float range_high);

/**
 * @brief Sets the argus camera gain range.
 *
 * Calls VIDIOC_S_EXT_CTRLS IOCTL internally with Control ID
 * \c V4L2_CID_ARGUS_GAIN_RANGE. Must be called
 * after setting format on capture plane.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] range_low Lower limit value.
 * @param[in] range_high Higher limit value.
 *
 * @return 0 for success, -1 otherwise.
 */
int set_gain_range (int fd, float range_low, float range_high);

/**
 * @brief Sets the argus camera digital gain range.
 *
 * Calls VIDIOC_S_EXT_CTRLS IOCTL internally with Control ID
 * \c V4L2_CID_ARGUS_ISP_DIGITAL_GAIN_RANGE. Must be called
 * after setting format on capture plane.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] range_low Lower limit value.
 * @param[in] range_high Higher limit value.
 *
 * @return 0 for success, -1 otherwise.
 */
int set_digital_gain_range (int fd, float range_low, float range_high);

/**
 * @brief Sets the argus camera digital gain range.
 *
 * Calls VIDIOC_S_EXT_CTRLS IOCTL internally with Control ID
 * \c V4L2_CID_ARGUS_COLOR_SATURATION. Must be called
 * after setting format on capture plane.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] saturation color value.
 *
 * @return 0 for success, -1 otherwise.
 */

int set_color_saturation (int fd, float saturation);

/***
 * @brief Gets argus metata
 * Calls VIDIOC_G_EXT_CTRLS IOCTL internally with Control ID
 * \c V4L2_CID_ARGUS_METADATA.
 *
 * @param[in] fd Argusv4l2 context FD.
 * @param[in] index v4l2 buffer index
 *
 * @return 0 for success, -1 otherwise
*/
int get_metadata(int fd, int index);

#endif /* __CAMERA_SAMPLE__CTRLS_H__ */
