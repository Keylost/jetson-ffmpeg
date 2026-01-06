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

#include "CommonOptions.h"
#include "ArgusHelpers.h"

namespace ArgusSamples
{

static const uint32_t DEFAULT_CAMERA_DEVICE   = 0;
static const uint32_t DEFAULT_SENSOR_MODE     = 0;
static const uint32_t DEFAULT_CAPTURE_TIME    = 5;
static const uint32_t DEFAULT_FRAME_COUNT     = 60;
static const uint32_t DEFAULT_FRAME_RATE      = 30;
static const std::string DEFAULT_PIXEL_FORMAT = "yuv444";
static const Argus::Rectangle<uint32_t> DEFAULT_WINDOW_RECT(0, 0, 1024, 768);

CommonOptions::CommonOptions(const char *programName, uint32_t optionEnables)
    : Options(programName)
    , m_optionEnables(optionEnables)
    , m_cameraDeviceIndex(DEFAULT_CAMERA_DEVICE)
    , m_sensorModeIndex(DEFAULT_SENSOR_MODE)
    , m_pixelFormatIndex(DEFAULT_PIXEL_FORMAT)
    , m_captureTime(DEFAULT_CAPTURE_TIME)
    , m_frameCount(DEFAULT_FRAME_COUNT)
    , m_frameRate(DEFAULT_FRAME_RATE)
    , m_windowRect(DEFAULT_WINDOW_RECT)
{
    printf("Executing Argus Sample: %s\n", programName);

    addOption(Option("listdevices", 'l', "",
            Option::TYPE_ACTION,
            "List all available CameraDevices, then exit", listCameraDevices, this));

    if (m_optionEnables & Option_D_CameraDevice)
    {
        addOption(createValueOption
            ("device", 'd', "INDEX", "Camera Device index.", m_cameraDeviceIndex));
    }
    if (m_optionEnables & Option_M_SensorMode)
    {
        addOption(createValueOption
            ("mode", 'm', "INDEX", "Sensor Mode index.", m_sensorModeIndex));
    }
    if (m_optionEnables & Option_R_WindowRect)
    {
        addOption(createValueOption
            ("rect", 'r', "WINDOW", "Window rectangle.", m_windowRect));
    }
    if (m_optionEnables & Option_T_CaptureTime)
    {
        addOption(createValueOption
            ("duration", 't', "SECONDS", "Capture duration.", m_captureTime));
    }
    if (m_optionEnables & Option_F_FrameCount)
    {
        addOption(createValueOption
            ("frames", 'f', "COUNT", "Frame count.", m_frameCount));
    }
    if (m_optionEnables & Option_P_PixelFormat)
    {
        addOption(createValueOption
            ("pixel", 'p', "yuv444/yuv420", "Pixel Format Index.", m_pixelFormatIndex, "yuv444"));
    }
    if (m_optionEnables & Option_FPS_FrameRate)
    {
        addOption(createValueOption
            ("frameRate", 'F', "FPS", "Frame rate per second.", m_frameRate));
    }
}

CommonOptions::~CommonOptions()
{
}

bool CommonOptions::parse(const int argc, char * const *argv)
{
    bool results = Options::parse(argc, argv);

    if ((m_optionEnables & Option_R_WindowRect) && (windowRect().width() % 2))
    {
        printf("WARNING: Window rect has an odd width. If the Argus OutputStream is sized\n"
               "         according to the window rect, using an odd size may fail.\n");
    }

    return results;
}

/* static */
bool CommonOptions::listCameraDevices(void *userPtr, const char *optArg)
{
    CommonOptions *options = reinterpret_cast<CommonOptions*>(userPtr);

    Argus::UniqueObj<Argus::CameraProvider> cameraProvider(Argus::CameraProvider::create());
    Argus::ICameraProvider *iCameraProvider =
        Argus::interface_cast<Argus::ICameraProvider>(cameraProvider);
    if (iCameraProvider)
    {
        std::vector<Argus::CameraDevice*> cameraDevices;
        iCameraProvider->getCameraDevices(&cameraDevices);
        printf("%lu Available CameraDevices:\n\n",
               static_cast<unsigned long>(cameraDevices.size()));
        for (uint32_t i = 0; i < cameraDevices.size(); i++)
        {
            printf("  ==== CameraDevice %u: =========================================\n", i);
            ArgusHelpers::printCameraDeviceInfo(cameraDevices[i], "    ");
            printf("\n");
        }
    }

    options->requestExit();

    return true;
}

}; // namespace ArgusSamples
