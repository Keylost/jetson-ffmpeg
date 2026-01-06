/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
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

#ifndef COMMON_OPTIONS_H
#define COMMON_OPTIONS_H

#include "Options.h"

namespace ArgusSamples
{

/**
 * Many of the Argus samples use similar command line options, such as options
 * to select the camera device, sensor mode, window rect, or sample runtime.
 * This CommonOptions class provides a base implementation for these common
 * options, as well as some utility functions to simplify the samples that
 * are using these options. This provides a standard and consistent set of
 * of command line parameters for all samples.
 * This class may be extended if additional parameters are needed for a sample.
 */
class CommonOptions : public Options
{
public:

    /**
     * List of options that may be provided by the CommonOptions. The letter in the flag
     * name corresponds to the letter option that is used for that command line option.
     */
    enum OptionFlags
    {
        /// The following common options must be explicitly enabled by each sample that
        /// uses them, to ensure that they're actually handled by the sample
        /// (eg. we shouldn't enable the WindowRect option for windowless samples).
        Option_D_CameraDevice = (1 << 0),
        Option_M_SensorMode   = (1 << 1),
        Option_R_WindowRect   = (1 << 2),
        Option_T_CaptureTime  = (1 << 3),
        Option_F_FrameCount   = (1 << 4),
        Option_P_PixelFormat  = (1 << 5),
        Option_FPS_FrameRate  = (1 << 6),

        /// These options are always enabled in the CommonOptions.
        Option_L_ListDevices    = (1 << 28),

        /// These actions are added by the base Options class (defined here to reserve letters).
        Option_O_DisplayChanges = (1 << 29),
        Option_H_Help           = (1 << 30),
        Option_X_Exit           = (1 << 31),

        Option_MAX              = (1 << 31)
    };

    /**
     * Constructs a CommonOptions object.
     * @param[in] programName The program's executable name.
     * @param[in] optionEnables Bitfield of optional options to be enabled (see OptionsFlags).
     */
    explicit CommonOptions(const char *programName, uint32_t optionEnables);
    virtual ~CommonOptions();

    /** @name Options methods */
    virtual bool parse(const int argc, char * const *argv);
    /**@}*/

    uint32_t cameraDeviceIndex() const
    {
        assert(m_optionEnables & Option_D_CameraDevice);
        return m_cameraDeviceIndex.get();
    }
    uint32_t sensorModeIndex() const
    {
        assert(m_optionEnables & Option_M_SensorMode);
        return m_sensorModeIndex.get();
    }
    uint32_t captureTime() const
    {
        assert(m_optionEnables & Option_T_CaptureTime);
        return m_captureTime.get();
    }
    uint32_t frameCount() const
    {
        assert(m_optionEnables & Option_F_FrameCount);
        return m_frameCount.get();
    }
    uint32_t pixelFormatIndex() const
    {
        assert(m_optionEnables & Option_P_PixelFormat);
        return strcmp(m_pixelFormatIndex.get().c_str(), "yuv420")==0;
    }
    uint32_t FPS() const
    {
        assert(m_optionEnables & Option_FPS_FrameRate);
        return m_frameRate.get();
    }
    const Argus::Rectangle<uint32_t>& windowRect() const
    {
        assert(m_optionEnables & Option_R_WindowRect);
        return m_windowRect.get();
    }

protected:

    // Callback for '-l' option to list available CameraDevices then exit.
    static bool listCameraDevices(void *userPtr, const char *optArg);

    uint32_t m_optionEnables;
    Value<uint32_t> m_cameraDeviceIndex;
    Value<uint32_t> m_sensorModeIndex;
    Value<std::string> m_pixelFormatIndex;
    Value<uint32_t> m_captureTime;
    Value<uint32_t> m_frameCount;
    Value<uint32_t> m_frameRate;
    Value<Argus::Rectangle<uint32_t> > m_windowRect;
};

}; // namespace ArgusSamples

#endif // COMMON_OPTIONS_H
