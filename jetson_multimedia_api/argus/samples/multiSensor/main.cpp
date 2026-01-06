/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION. All rights reserved.
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

/*
 * This sample opens two independent camera sessions using 2 sensors it then uses the first sensor
 * to display a preview on the screen, while taking jpeg snapshots every second from the second
 * sensor. The Jpeg saving and Preview consumption happen on two consumer threads in the
 * PreviewConsumerThread and JPEGConsumerThread classes, located in the util folder.
 */

namespace ArgusSamples
{

// Globals and derived constants.
EGLDisplayHolder g_display;

// Debug print macros.
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)

/*******************************************************************************
 * Extended options class to add additional options specific to this sample.
 ******************************************************************************/
class MultiSensorSampleOptions : public CommonOptions
{
public:
    MultiSensorSampleOptions(const char *programName)
        : CommonOptions(programName,
                        ArgusSamples::CommonOptions::Option_D_CameraDevice |
                        ArgusSamples::CommonOptions::Option_R_WindowRect |
                        ArgusSamples::CommonOptions::Option_T_CaptureTime)
        , m_jpegDeviceIndex(1)
    {
        addOption(createValueOption
            ("jpeg", 'j', "INDEX", "Camera device index of JPEG stream "
             "(use --device to set device index for preview stream).", m_jpegDeviceIndex));
    }

    uint32_t jpegDeviceIndex() const { return m_jpegDeviceIndex.get(); }

protected:
    Value<uint32_t> m_jpegDeviceIndex;
};

static bool execute(const MultiSensorSampleOptions& options)
{
    // Initialize the window and EGL display.
    Window &window = Window::getInstance();
    window.setWindowRect(options.windowRect());
    PROPAGATE_ERROR(g_display.initialize(window.getEGLNativeDisplay()));

    // Initialize the Argus camera provider.
    UniqueObj<CameraProvider> cameraProvider(CameraProvider::create());

    // Get the ICameraProvider interface from the global CameraProvider.
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to get ICameraProvider interface");
    printf("Argus Version: %s\n", iCameraProvider->getVersion().c_str());

    // Get the camera devices.
    std::vector<CameraDevice*> cameraDevices;
    iCameraProvider->getCameraDevices(&cameraDevices);
    if (options.cameraDeviceIndex() == options.jpegDeviceIndex())
    {
        ORIGINATE_ERROR("Preview and JPEG camera indexes may not be the same");
    }
    if (cameraDevices.size() <= options.cameraDeviceIndex())
    {
        ORIGINATE_ERROR("Preview Camera index %d not available; there are %d cameras",
                        options.cameraDeviceIndex(), (unsigned)cameraDevices.size());
    }
    if (cameraDevices.size() <= options.jpegDeviceIndex())
    {
        ORIGINATE_ERROR("JPEG camera index %d not available; there are %d cameras",
                        options.jpegDeviceIndex(), (unsigned)cameraDevices.size());
    }
    Argus::CameraDevice* previewDevice = cameraDevices[options.cameraDeviceIndex()];
    Argus::CameraDevice* jpegDevice = cameraDevices[options.jpegDeviceIndex()];

    // Get the default SensorMode from the JPEG device to determine the stream size
    // (the window rect size is used for the preview stream size).
    ICameraProperties *iJpegDeviceProperties = interface_cast<ICameraProperties>(jpegDevice);
    if (!iJpegDeviceProperties)
    {
        ORIGINATE_ERROR("Failed to get the JPEG camera device properties interface.");
    }
    std::vector<Argus::SensorMode*> jpegSensorModes;
    iJpegDeviceProperties->getBasicSensorModes(&jpegSensorModes);
    if (!jpegSensorModes.size())
    {
        ORIGINATE_ERROR("Failed to get valid JPEG sensor mode list.");
    }
    ISensorMode *iJpegSensorMode = interface_cast<ISensorMode>(jpegSensorModes[0]);
    if (!iJpegSensorMode)
        ORIGINATE_ERROR("Failed to get the sensor mode.");

    // Create the JPEG capture session.
    UniqueObj<CaptureSession> jpegSession = UniqueObj<CaptureSession>(
            iCameraProvider->createCaptureSession(jpegDevice));
    if (!jpegSession)
        ORIGINATE_ERROR(
            "Failed to create JPEG session with camera index %d.", options.jpegDeviceIndex());
    ICaptureSession *iJpegCaptureSession = interface_cast<ICaptureSession>(jpegSession);
    if (!iJpegCaptureSession)
        ORIGINATE_ERROR("Failed to get JPEG capture session interface");

    // Create the preview capture session.
    UniqueObj<CaptureSession> previewSession = UniqueObj<CaptureSession>(
            iCameraProvider->createCaptureSession(previewDevice));
    if (!previewSession)
        ORIGINATE_ERROR(
            "Failed to create preview session with camera index %d.", options.cameraDeviceIndex());
    ICaptureSession *iPreviewCaptureSession = interface_cast<ICaptureSession>(previewSession);
    if (!iPreviewCaptureSession)
        ORIGINATE_ERROR("Failed to get preview capture session interface");

    // Create preview stream.
    PRODUCER_PRINT("Creating the preview stream.\n");
    UniqueObj<OutputStreamSettings> previewSettings(
        iPreviewCaptureSession->createOutputStreamSettings(STREAM_TYPE_EGL));
    IEGLOutputStreamSettings *iPreviewSettings =
        interface_cast<IEGLOutputStreamSettings>(previewSettings);
    if (iPreviewSettings)
    {
        iPreviewSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
        iPreviewSettings->setResolution(Size2D<uint32_t>(options.windowRect().width(),
                                                         options.windowRect().height()));
        iPreviewSettings->setEGLDisplay(g_display.get());
    }
    UniqueObj<OutputStream> previewStream(
            iPreviewCaptureSession->createOutputStream(previewSettings.get()));
    IEGLOutputStream *iPreviewStream = interface_cast<IEGLOutputStream>(previewStream);
    if (!iPreviewStream)
        ORIGINATE_ERROR("Failed to create preview OutputStream");

    PRODUCER_PRINT("Launching preview consumer thread\n");
    PreviewConsumerThread previewConsumer(g_display.get(), iPreviewStream->getEGLStream());
    PROPAGATE_ERROR(previewConsumer.initialize());
    PROPAGATE_ERROR(previewConsumer.waitRunning());

    // Create JPEG stream.
    PRODUCER_PRINT("Creating the JPEG stream.\n");
    UniqueObj<OutputStreamSettings> jpegSettings(
        iJpegCaptureSession->createOutputStreamSettings(STREAM_TYPE_EGL));
    IEGLOutputStreamSettings *iJpegSettings =
        interface_cast<IEGLOutputStreamSettings>(jpegSettings);
    if (iJpegSettings)
    {
        iJpegSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
        iJpegSettings->setResolution(iJpegSensorMode->getResolution());
        iJpegSettings->setEGLDisplay(g_display.get());
        iJpegSettings->setMetadataEnable(true);
    }
    UniqueObj<OutputStream> jpegStream(
            iJpegCaptureSession->createOutputStream(jpegSettings.get()));
    if (!jpegStream.get())
        ORIGINATE_ERROR("Failed to create JPEG OutputStream");

    JPEGConsumerThread jpegConsumer(jpegStream.get());
    PROPAGATE_ERROR(jpegConsumer.initialize());
    PROPAGATE_ERROR(jpegConsumer.waitRunning());

    // Create the two requests
    UniqueObj<Request> previewRequest(iPreviewCaptureSession->createRequest());
    UniqueObj<Request> jpegRequest(iJpegCaptureSession->createRequest());
    if (!previewRequest || !jpegRequest)
        ORIGINATE_ERROR("Failed to create Request");

    IRequest *iPreviewRequest = interface_cast<IRequest>(previewRequest);
    IRequest *iJpegRequest = interface_cast<IRequest>(jpegRequest);
    if (!iPreviewRequest || !iJpegRequest)
        ORIGINATE_ERROR("Failed to create Request interface");

    iPreviewRequest->enableOutputStream(previewStream.get());
    iJpegRequest->enableOutputStream(jpegStream.get());

    // Argus is now all setup and ready to capture

    // Submit capture requests.
    PRODUCER_PRINT("Starting repeat capture requests.\n");

    // Start the preview
    if (iPreviewCaptureSession->repeat(previewRequest.get()) != STATUS_OK)
        ORIGINATE_ERROR("Failed to start repeat capture request for preview");

    // Wait for the specified number of seconds and do a JPEG capture every second.
    for (uint32_t i = 0; i < options.captureTime(); i++)
    {
        if (iJpegCaptureSession->capture(jpegRequest.get()) == 0)
            ORIGINATE_ERROR("Failed to submit JPEG capture request");
        PROPAGATE_ERROR(window.pollEvents());
        sleep(1);
    }
    window.pollEvents();

    // all done shut down
    iJpegCaptureSession->stopRepeat();
    iPreviewCaptureSession->stopRepeat();
    iJpegCaptureSession->waitForIdle();
    iPreviewCaptureSession->waitForIdle();

    previewStream.reset();
    jpegStream.reset();

    // Wait for the consumer threads to complete.
    PROPAGATE_ERROR(previewConsumer.shutdown());
    PROPAGATE_ERROR(jpegConsumer.shutdown());

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
    ArgusSamples::MultiSensorSampleOptions options(basename(argv[0]));
    if (!options.parse(argc, argv))
        return EXIT_FAILURE;
    if (options.requestedExit())
        return EXIT_SUCCESS;

    if (!ArgusSamples::execute(options))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
