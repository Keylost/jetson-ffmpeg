/*
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
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

#include <Argus/Argus.h>

#include "ArgusHelpers.h"
#include "CommonOptions.h"
#include "CudaBayerDemosaicConsumer.h"

namespace ArgusSamples
{

// Globals and derived constants.
EGLDisplayHolder g_display;

/**
 * Main thread function opens connection to Argus driver, creates a capture session for
 * a given camera device and sensor mode, then creates a RAW16 stream attached to a
 * CudaBayerConsumer such that the CUDA consumer will acquire the outputs of capture
 * results as raw Bayer data (which it then demosaics to RGBA for demonstration purposes).
 */
static bool execute(const CommonOptions& options)
{
    // Initialize the preview window and EGL display.
    Window &window = Window::getInstance();
    window.setWindowRect(options.windowRect());
    PROPAGATE_ERROR(g_display.initialize(window.getEGLNativeDisplay()));

    // Create the Argus CameraProvider object
    UniqueObj<CameraProvider> cameraProvider(CameraProvider::create());
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(cameraProvider);
    if (!iCameraProvider)
    {
        ORIGINATE_ERROR("Failed to create CameraProvider");
    }
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

    // Create the capture session using the selected device.
    UniqueObj<CaptureSession> captureSession(iCameraProvider->createCaptureSession(cameraDevice));
    ICaptureSession *iCaptureSession = interface_cast<ICaptureSession>(captureSession);
    if (!iCaptureSession)
    {
        ORIGINATE_ERROR("Failed to create CaptureSession");
    }

    // Create the RAW16 output EGLStream using the sensor mode resolution.
    UniqueObj<OutputStreamSettings> streamSettings(
        iCaptureSession->createOutputStreamSettings(STREAM_TYPE_EGL));
    IEGLOutputStreamSettings *iEGLStreamSettings =
        interface_cast<IEGLOutputStreamSettings>(streamSettings);
    if (!iEGLStreamSettings)
    {
        ORIGINATE_ERROR("Failed to create OutputStreamSettings");
    }
    iEGLStreamSettings->setEGLDisplay(g_display.get());
    iEGLStreamSettings->setPixelFormat(PIXEL_FMT_RAW16);
    iEGLStreamSettings->setResolution(iSensorMode->getResolution());
    iEGLStreamSettings->setMode(EGL_STREAM_MODE_FIFO);
    UniqueObj<OutputStream> outputStream(iCaptureSession->createOutputStream(streamSettings.get()));
    IEGLOutputStream *iEGLOutputStream = interface_cast<IEGLOutputStream>(outputStream);
    if (!iEGLOutputStream)
    {
        ORIGINATE_ERROR("Failed to create EGLOutputStream");
    }

    // Create capture request and enable output stream.
    UniqueObj<Request> request(iCaptureSession->createRequest());
    IRequest *iRequest = interface_cast<IRequest>(request);
    if (!iRequest)
    {
        ORIGINATE_ERROR("Failed to create Request");
    }
    iRequest->enableOutputStream(outputStream.get());

    // Set the sensor mode in the request.
    ISourceSettings *iSourceSettings = interface_cast<ISourceSettings>(request);
    if (!iSourceSettings)
        ORIGINATE_ERROR("Failed to get source settings request interface");
    iSourceSettings->setSensorMode(sensorMode);

    // Create the CUDA Bayer consumer and connect it to the RAW16 output stream.
    CudaBayerDemosaicConsumer cudaConsumer(iEGLOutputStream->getEGLDisplay(),
                                           iEGLOutputStream->getEGLStream(),
                                           iEGLStreamSettings->getResolution(),
                                           options.frameCount());
    PROPAGATE_ERROR(cudaConsumer.initialize());
    PROPAGATE_ERROR(cudaConsumer.waitRunning());

    // Submit the batch of capture requests.
    for (unsigned int frame = 0; frame < options.frameCount(); ++frame)
    {
        Argus::Status status;
        uint32_t result = iCaptureSession->capture(request.get(), TIMEOUT_INFINITE, &status);
        if (result == 0)
        {
            ORIGINATE_ERROR("Failed to submit capture request (status %x)", status);
        }
    }

    // Wait until all captures have completed.
    iCaptureSession->waitForIdle();

    // Shutdown the CUDA consumer.
    PROPAGATE_ERROR(cudaConsumer.shutdown());

    // Shut down Argus.
    cameraProvider.reset();

    // Shut down the window (destroys window's EGLSurface).
    window.shutdown();

    return true;
}

}; // namespace ArgusSamples

int main(int argc, char** argv)
{
    printf("Executing Argus Sample: %s\n", basename(argv[0]));

    ArgusSamples::CommonOptions options(basename(argv[0]),
                                        ArgusSamples::CommonOptions::Option_D_CameraDevice |
                                        ArgusSamples::CommonOptions::Option_M_SensorMode |
                                        ArgusSamples::CommonOptions::Option_R_WindowRect |
                                        ArgusSamples::CommonOptions::Option_F_FrameCount);
    if (!options.parse(argc, argv))
        return EXIT_FAILURE;
    if (options.requestedExit())
        return EXIT_SUCCESS;

    if (!ArgusSamples::execute(options))
        return EXIT_FAILURE;

    printf("Argus sample '%s' completed successfully.\n", basename(argv[0]));

    return EXIT_SUCCESS;
}
