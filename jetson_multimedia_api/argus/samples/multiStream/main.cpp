/*
 * Copyright (c) 2016 - 2018, NVIDIA CORPORATION. All rights reserved.
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

#include "ArgusHelpers.h"
#include "CommonOptions.h"
#include "Error.h"
#include "EGLGlobal.h"
#include "GLContext.h"
#include "JPEGConsumer.h"
#include "PreviewConsumer.h"
#include "Window.h"
#include "Thread.h"

#include <Argus/Argus.h>

#include <unistd.h>
#include <stdlib.h>
#include <sstream>
#include <iomanip>

using namespace Argus;

namespace ArgusSamples
{

// Globals.
EGLDisplayHolder g_display;

// Debug print macros.
#define PRODUCER_PRINT(...)         printf("PRODUCER: " __VA_ARGS__)

/*******************************************************************************
 * Extended options class to add additional options specific to this sample.
 ******************************************************************************/
class MultiStreamSampleOptions : public CommonOptions
{
public:
    MultiStreamSampleOptions(const char *programName)
        : CommonOptions(programName,
                        ArgusSamples::CommonOptions::Option_D_CameraDevice |
                        ArgusSamples::CommonOptions::Option_M_SensorMode |
                        ArgusSamples::CommonOptions::Option_R_WindowRect |
                        ArgusSamples::CommonOptions::Option_T_CaptureTime)
        , m_jpegInterval(3)
    {
        addOption(createValueOption
            ("jpegInterval", 'j', "INTERVAL", "Frame interval for JPEG writes.", m_jpegInterval));
    }

    uint32_t jpegInterval() const { return m_jpegInterval.get(); }

protected:
    Value<uint32_t> m_jpegInterval;
};

/*******************************************************************************
 * Argus Producer thread:
 *   Opens the Argus camera driver, creates two OutputStreams -- one for live
 *   preview to display and the other to write JPEG files -- and submits capture
 *   requests. Burst captures are used such that the JPEG stream is only written
 *   to once using the provided JPEG interval option.
 ******************************************************************************/
static bool execute(const MultiStreamSampleOptions& options)
{
    // Initialize the window and EGL display.
    Window &window = Window::getInstance();
    window.setWindowRect(options.windowRect());
    PROPAGATE_ERROR(g_display.initialize(window.getEGLNativeDisplay()));

    // Initialize the Argus camera provider.
    UniqueObj<CameraProvider> cameraProvider(CameraProvider::create());
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to get ICameraProvider interface");
    printf("Argus Version: %s\n", iCameraProvider->getVersion().c_str());

    // Get the selected camera device and sensor mode.
    CameraDevice* cameraDevice = ArgusHelpers::getCameraDevice(
            cameraProvider.get(), options.cameraDeviceIndex());
    if (!cameraDevice)
        ORIGINATE_ERROR("Selected camera device is not available");
    SensorMode* sensorMode = ArgusHelpers::getSensorMode(cameraDevice, options.sensorModeIndex());
    ISensorMode *iSensorMode = interface_cast<ISensorMode>(sensorMode);
    if (!iSensorMode)
        ORIGINATE_ERROR("Selected sensor mode not available");

    // Create the capture session.
    UniqueObj<CaptureSession> captureSession(iCameraProvider->createCaptureSession(cameraDevice));
    ICaptureSession *iCaptureSession = interface_cast<ICaptureSession>(captureSession);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to create CaptureSession");

    // Create the stream settings and set the common properties.
    UniqueObj<OutputStreamSettings> streamSettings(
        iCaptureSession->createOutputStreamSettings(STREAM_TYPE_EGL));
    IEGLOutputStreamSettings *iStreamSettings =
        interface_cast<IEGLOutputStreamSettings>(streamSettings);
    if (!iStreamSettings)
        ORIGINATE_ERROR("Failed to create OutputStreamSettings");
    iStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
    iStreamSettings->setEGLDisplay(g_display.get());

    // Create window rect sized OutputStream that is consumed by the preview (OpenGL) consumer.
    PRODUCER_PRINT("Creating preview output stream\n");
    iStreamSettings->setResolution(Size2D<uint32_t>(options.windowRect().width(),
                                                    options.windowRect().height()));
    UniqueObj<OutputStream> previewStream(
            iCaptureSession->createOutputStream(streamSettings.get()));
    IEGLOutputStream *iPreviewStream = interface_cast<IEGLOutputStream>(previewStream);
    if (!iPreviewStream)
        ORIGINATE_ERROR("Failed to create OutputStream");

    PRODUCER_PRINT("Launching preview consumer thread\n");
    PreviewConsumerThread previewConsumerThread(iPreviewStream->getEGLDisplay(),
                                                iPreviewStream->getEGLStream());
    PROPAGATE_ERROR(previewConsumerThread.initialize());
    PROPAGATE_ERROR(previewConsumerThread.waitRunning());

    // Create a full-resolution OutputStream that is consumed by the JPEG Consumer.
    PRODUCER_PRINT("Creating JPEG output stream\n");
    iStreamSettings->setResolution(iSensorMode->getResolution());
    iStreamSettings->setMetadataEnable(true);

    UniqueObj<OutputStream> jpegStream(iCaptureSession->createOutputStream(streamSettings.get()));
    if (!jpegStream)
        ORIGINATE_ERROR("Failed to create OutputStream");

    PRODUCER_PRINT("Launching JPEG consumer thread\n");
    JPEGConsumerThread jpegConsumerThread(jpegStream.get());
    PROPAGATE_ERROR(jpegConsumerThread.initialize());
    PROPAGATE_ERROR(jpegConsumerThread.waitRunning());

    // Create the capture requests.
    std::vector<const Argus::Request*> requests;
    for (uint32_t i = 0; i < options.jpegInterval(); i++)
    {
        Request *request = iCaptureSession->createRequest();
        IRequest *iRequest = interface_cast<IRequest>(request);
        ISourceSettings *iSourceSettings = interface_cast<ISourceSettings>(request);
        if (!iRequest || !iSourceSettings)
            ORIGINATE_ERROR("Failed to create Request");
        requests.push_back(request);

        // Set the sensor mode in the request.
        iSourceSettings->setSensorMode(sensorMode);

        // Enable the preview stream for every capture in the burst.
        iRequest->enableOutputStream(previewStream.get());

        // Enable the JPEG stream for only the first capture in the burst.
        if (i == 0)
        {
            iRequest->enableOutputStream(jpegStream.get());

            // The internal post-processing pipeline is generated on a per-request basis,
            // and is dependent on the full set of enabled output streams that have
            // post-processing enabled. In order to prevent these pipeline changes,
            // which may cause visual changes in the preview stream, post-processing
            // is disabled for the periodic still capture.
            IStreamSettings *jpegStreamSettings =
                interface_cast<IStreamSettings>(iRequest->getStreamSettings(jpegStream.get()));
            jpegStreamSettings->setPostProcessingEnable(false);
        }
    }

    if (iCaptureSession->repeatBurst(requests) != STATUS_OK)
        ORIGINATE_ERROR("Failed to start repeat burst capture request");

    // Wait for CAPTURE_TIME seconds.
    PROPAGATE_ERROR(window.pollingSleep(options.captureTime()));

    // Stop the repeating request and wait for idle.
    iCaptureSession->stopRepeat();
    iCaptureSession->waitForIdle();

    // Destroy the requests.
    for (uint32_t i = 0; i < requests.size(); i++)
    {
        const_cast<Request*>(requests[i])->destroy();
    }
    requests.clear();

    // Destroy the output streams (stops consumer threads).
    previewStream.reset();
    jpegStream.reset();

    // Wait for the consumer threads to complete.
    PROPAGATE_ERROR(previewConsumerThread.shutdown());
    PROPAGATE_ERROR(jpegConsumerThread.shutdown());

    // Shut down Argus.
    cameraProvider.reset();

    // Shut down the window (destroys window's EGLSurface).
    window.shutdown();

    // Cleanup the EGL display
    PROPAGATE_ERROR(g_display.cleanup());

    PRODUCER_PRINT("Done -- exiting.\n");

    return true;
}

}; // namespace ArgusSamples

int main(int argc, char** argv)
{
    ArgusSamples::MultiStreamSampleOptions options(basename(argv[0]));
    if (!options.parse(argc, argv))
        return EXIT_FAILURE;
    if (options.requestedExit())
        return EXIT_SUCCESS;

    if (!ArgusSamples::execute(options))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
