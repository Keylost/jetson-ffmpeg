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

#include <stdio.h>
#include <stdlib.h>
#include <Argus/Argus.h>
#include <EGLStream/EGLStream.h>
#include "ArgusHelpers.h"
#include "CommonOptions.h"

#define EXIT_IF_NULL(val,msg)   \
        {if (!val) {printf("%s\n",msg); return EXIT_FAILURE;}}
#define EXIT_IF_NOT_OK(val,msg) \
        {if (val!=Argus::STATUS_OK) {printf("%s\n",msg); return EXIT_FAILURE;}}

#ifdef ANDROID
#define FILE_PREFIX "/sdcard/DCIM/"
#else
#define FILE_PREFIX ""
#endif

/*
 * Program: oneShot
 * Function: Capture a single image from a camera device and write to a JPG file
 * Purpose: To demonstrate the most simplistic approach to getting the Argus Framework
 *          running, submitting a capture request, retrieving the resulting image and
 *          then writing the image as a JPEG formatted file.
 */

int main(int argc, char** argv)
{
    ArgusSamples::CommonOptions options(basename(argv[0]),
                                        ArgusSamples::CommonOptions::Option_D_CameraDevice |
                                        ArgusSamples::CommonOptions::Option_M_SensorMode);
    if (!options.parse(argc, argv))
        return EXIT_FAILURE;
    if (options.requestedExit())
        return EXIT_SUCCESS;

    const uint64_t FIVE_SECONDS_IN_NANOSECONDS = 5000000000;

    /*
     * Set up Argus API Framework, identify available camera devices, and create
     * a capture session for the first available device
     */

    Argus::UniqueObj<Argus::CameraProvider> cameraProvider(Argus::CameraProvider::create());

    Argus::ICameraProvider *iCameraProvider =
        Argus::interface_cast<Argus::ICameraProvider>(cameraProvider);
    EXIT_IF_NULL(iCameraProvider, "Cannot get core camera provider interface");
    printf("Argus Version: %s\n", iCameraProvider->getVersion().c_str());

    Argus::CameraDevice *device = ArgusSamples::ArgusHelpers::getCameraDevice(
            cameraProvider.get(), options.cameraDeviceIndex());
    Argus::ICameraProperties *iCameraProperties =
        Argus::interface_cast<Argus::ICameraProperties>(device);
    if (!iCameraProperties)
    {
        REPORT_ERROR("Failed to get ICameraProperties interface");
        return EXIT_FAILURE;
    }

    Argus::SensorMode* sensorMode = ArgusSamples::ArgusHelpers::getSensorMode(
            device, options.sensorModeIndex());
    Argus::ISensorMode *iSensorMode =
        Argus::interface_cast<Argus::ISensorMode>(sensorMode);
    if (!iSensorMode)
    {
        REPORT_ERROR("Failed to get sensor mode interface");
        return EXIT_FAILURE;
    }

    printf("Capturing from device %d using sensor mode %d (%dx%d)\n",
           options.cameraDeviceIndex(), options.sensorModeIndex(),
           iSensorMode->getResolution().width(), iSensorMode->getResolution().height());

    Argus::Status status;
    Argus::UniqueObj<Argus::CaptureSession> captureSession(
        iCameraProvider->createCaptureSession(device, &status));
    EXIT_IF_NOT_OK(status, "Failed to create capture session");

    Argus::ICaptureSession *iSession =
        Argus::interface_cast<Argus::ICaptureSession>(captureSession);
    EXIT_IF_NULL(iSession, "Cannot get Capture Session Interface");

    /*
     * Creates the stream between the Argus camera image capturing
     * sub-system (producer) and the image acquisition code (consumer).  A consumer object is
     * created from the stream to be used to request the image frame.  A successfully submitted
     * capture request activates the stream's functionality to eventually make a frame available
     * for acquisition.
     */

    Argus::UniqueObj<Argus::OutputStreamSettings> streamSettings(
        iSession->createOutputStreamSettings(Argus::STREAM_TYPE_EGL));

    Argus::IEGLOutputStreamSettings *iEGLStreamSettings =
        Argus::interface_cast<Argus::IEGLOutputStreamSettings>(streamSettings);
    EXIT_IF_NULL(iEGLStreamSettings, "Cannot get IEGLOutputStreamSettings Interface");
    iEGLStreamSettings->setPixelFormat(Argus::PIXEL_FMT_YCbCr_420_888);
    iEGLStreamSettings->setResolution(iSensorMode->getResolution());
    iEGLStreamSettings->setMetadataEnable(true);

    Argus::UniqueObj<Argus::OutputStream> stream(
        iSession->createOutputStream(streamSettings.get()));
    EXIT_IF_NULL(stream, "Failed to create EGLOutputStream");

    Argus::UniqueObj<EGLStream::FrameConsumer> consumer(
        EGLStream::FrameConsumer::create(stream.get()));

    EGLStream::IFrameConsumer *iFrameConsumer =
        Argus::interface_cast<EGLStream::IFrameConsumer>(consumer);
    EXIT_IF_NULL(iFrameConsumer, "Failed to initialize Consumer");

    Argus::UniqueObj<Argus::Request> request(
        iSession->createRequest(Argus::CAPTURE_INTENT_STILL_CAPTURE));

    Argus::IRequest *iRequest = Argus::interface_cast<Argus::IRequest>(request);
    EXIT_IF_NULL(iRequest, "Failed to get capture request interface");

    status = iRequest->enableOutputStream(stream.get());
    EXIT_IF_NOT_OK(status, "Failed to enable stream in capture request");

    Argus::ISourceSettings *iSourceSettings =
        Argus::interface_cast<Argus::ISourceSettings>(request);
    EXIT_IF_NULL(iSourceSettings, "Failed to get source settings request interface");
    iSourceSettings->setSensorMode(sensorMode);

    uint32_t requestId = iSession->capture(request.get());
    EXIT_IF_NULL(requestId, "Failed to submit capture request");

    /*
     * Acquire a frame generated by the capture request, get the image from the frame
     * and create a .JPG file of the captured image
     */
    Argus::UniqueObj<EGLStream::Frame> frame(
        iFrameConsumer->acquireFrame(FIVE_SECONDS_IN_NANOSECONDS, &status));

    EGLStream::IFrame *iFrame = Argus::interface_cast<EGLStream::IFrame>(frame);
    EXIT_IF_NULL(iFrame, "Failed to get IFrame interface");

    EGLStream::Image *image = iFrame->getImage();
    EXIT_IF_NULL(image, "Failed to get Image from iFrame->getImage()");

    EGLStream::IImageJPEG *iImageJPEG = Argus::interface_cast<EGLStream::IImageJPEG>(image);
    EXIT_IF_NULL(iImageJPEG, "Failed to get ImageJPEG Interface");

    status = iImageJPEG->writeJPEG(FILE_PREFIX "argus_oneShot.jpg");
    EXIT_IF_NOT_OK(status, "Failed to write JPEG");

    printf("Wrote file: " FILE_PREFIX "argus_oneShot.jpg\n");

    // Shut down Argus.
    cameraProvider.reset();

    return EXIT_SUCCESS;
}
