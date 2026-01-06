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

#include <stdio.h>
#include <getopt.h>
#include <sys/sysinfo.h>
#include <termios.h>
#include <Argus/Argus.h>
#include "StreamConsumer.h"
#include "VideoEncodeStreamConsumer.h"
#if ENABLE_TRT
#include "TRTStreamConsumer.h"
#endif
#include "Error.h"
#include "NvEglRenderer.h"

//
// This demo creates four camera input streams of different resolutions.
// Full resolution for TRT inference and other three for video encoding.
// TRT will detect target objects and user can see bounding box around
// these objects in preview.
// The app will run infinitely until user press 'q'.
//
using namespace Argus;
using namespace ArgusSamples;
using namespace EGLStream;

// Constant configuration.
static const int    CAPTURE_TIME = 1; // In seconds.
static const unsigned   MAX_STREAM  = 4;

// Configurations which can be overrided by cmdline
static std::string g_deployFile("../../data/Model/GoogleNet_three_class/GoogleNet_modified_threeClass_VGA.prototxt");
static std::string g_modelFile("../../data/Model/GoogleNet_three_class/GoogleNet_modified_threeClass_VGA.caffemodel");
static bool g_mode = false;
static bool g_bNoPreview = false;

// Globals.
static NvEglRenderer *g_eglRenderer = NULL;
bool g_bVerbose = false;
bool g_bProfiling = false;
UniqueObj<CameraProvider> g_cameraProvider;

// Debug print macros.
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)

#define MEMORY_2GB 2147483648
//
// Argus Producer thread
//   Opens the Argus camera driver, create several OutputStream, one for each
//   consumer, then performs repeating capture requests until user exits
//   Finally closing the producer and Argus driver.
//
static int
runArgusProducer(const std::vector<StreamConsumer*> &consumers)
{
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(g_cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to create CameraProvider");

    // Get the camera devices.
    std::vector<CameraDevice*> cameraDevices;
    iCameraProvider->getCameraDevices(&cameraDevices);
    if (cameraDevices.size() == 0)
        ORIGINATE_ERROR("No cameras available");

    // Create the capture session using the first device and get the core interface.
    UniqueObj<CaptureSession> captureSession(
            iCameraProvider->createCaptureSession(cameraDevices[0]));
    ICaptureSession *iCaptureSession = interface_cast<ICaptureSession>(captureSession);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to get ICaptureSession interface");

    // Create the OutputStream.
    PRODUCER_PRINT("Creating output stream\n");
    UniqueObj<OutputStreamSettings> streamSettings(
        iCaptureSession->createOutputStreamSettings(STREAM_TYPE_EGL));
    IEGLOutputStreamSettings *iEglStreamSettings =
        interface_cast<IEGLOutputStreamSettings>(streamSettings);
    if (!iEglStreamSettings)
        ORIGINATE_ERROR("Failed to get IEGLOutputStreamSettings interface");

    iEglStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);

    UniqueObj<OutputStream> outputStreams[MAX_STREAM];
    for (unsigned i = 0; i < consumers.size(); i++)
    {
        if (g_eglRenderer)
            iEglStreamSettings->setEGLDisplay(g_eglRenderer->getEGLDisplay());
        iEglStreamSettings->setResolution(consumers[i]->getSize());
        outputStreams[i].reset(iCaptureSession->createOutputStream(streamSettings.get()));
        consumers[i]->setOutputStream(outputStreams[i].get());
    }

    // Launch the FrameConsumer thread to consume frames from the OutputStream.
    PRODUCER_PRINT("Launching consumer thread\n");
    for (unsigned i = 0; i < consumers.size(); i++)
    {
        PROPAGATE_ERROR(consumers[i]->initialize());
    }

    // Wait until the consumer is connected to the stream.
    for (unsigned i = 0; i < consumers.size(); i++)
    {
        PROPAGATE_ERROR(consumers[i]->waitRunning());
    }

    // Create capture request and enable output stream.
    UniqueObj<Request> request(iCaptureSession->createRequest());
    IRequest *iRequest = interface_cast<IRequest>(request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to create Request");
    for (unsigned i = 0; i < consumers.size(); i++)
    {
        iRequest->enableOutputStream(outputStreams[i].get());
        IStreamSettings *iRequestStreamSettings =
            interface_cast<IStreamSettings>(iRequest->getStreamSettings(outputStreams[i].get()));
        if (!iRequestStreamSettings)
            ORIGINATE_ERROR("Failed to get IStreamSettings interface");
        iRequestStreamSettings->setPostProcessingEnable(false);
    }

    ISourceSettings *iSourceSettings = interface_cast<ISourceSettings>(iRequest->getSourceSettings());
    if (!iSourceSettings)
        ORIGINATE_ERROR("Failed to get ISourceSettings interface");
    iSourceSettings->setFrameDurationRange(Range<uint64_t>(1e9/30));

    // Submit capture requests.
    PRODUCER_PRINT("Starting repeat capture requests.\n");
    if (iCaptureSession->repeat(request.get()) != STATUS_OK)
        ORIGINATE_ERROR("Failed to start repeat capture request");

    // Wait until user press 'q'.
    while (getchar() != 'q');

    // Stop the repeating request and wait for idle.
    iCaptureSession->stopRepeat();
    iCaptureSession->waitForIdle();

    // Destroy the output stream to end the consumer thread.
    for (unsigned i = 0; i < consumers.size(); i++)
    {
        outputStreams[i].reset();
    }

    // Wait for the consumer thread to complete.
    for (unsigned i = 0; i < consumers.size(); i++)
    {
        PRODUCER_PRINT("Shutdown consumer %s\n", consumers[i]->getName());
        consumers[i]->shutdown();
    }

    PRODUCER_PRINT("Done -- exiting.\n");

    return true;
}

static bool getFullResolution(Size2D<uint32_t> *result)
{
    // Create the CameraProvider object and get the core interface.
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(g_cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to get ICameraProvider interface");

    // Get the camera devices.
    std::vector<CameraDevice*> cameraDevices;
    iCameraProvider->getCameraDevices(&cameraDevices);
    if (cameraDevices.size() == 0)
        ORIGINATE_ERROR("No cameras available");

    // Get camera properties.
    ICameraProperties *iCameraProperties = interface_cast<ICameraProperties>(cameraDevices[0]);
    if (!iCameraProperties)
        ORIGINATE_ERROR("Failed to get ICameraProperties interface");

    // Get available sensor modes.
    std::vector<SensorMode*> sensorModes;
    iCameraProperties->getBasicSensorModes(&sensorModes);

    // Select the mode of max resolution
    Size2D<uint32_t> maxSize(0, 0);
    for (unsigned i = 0; i < sensorModes.size(); i++)
    {
        ISensorMode *iSensorMode = interface_cast<ISensorMode>(sensorModes[i]);
        if (!iSensorMode)
            ORIGINATE_ERROR("Failed to get ISensorMode interface");

        Size2D<uint32_t> resolution = iSensorMode->getResolution();
        if (resolution.area() > maxSize.area())
            maxSize = resolution;
    }

    printf("Full resolution: %dx%d\n", maxSize.width(), maxSize.height());
    *result = maxSize;

    return true;
}

static void printHelp()
{
    printf("Usage: frontend [OPTIONS]\n"
           "Options:\n"
           "  -h        Print this help\n"
           "  --deploy <filename>   Sets deploy file\n"
           "  --model <filename>    Sets model file\n"
           "  --no-preview          Disables the renderer\n"
           "  --fp32                Force to use fp32\n"
           "  -s                    Enable profiling\n"
           "  -v                    Enable verbose message\n"
           "Commands\n"
           "  q:        exit\n");
}

static bool parseCmdline(int argc, char **argv)
{
    enum
    {
        OPTION_DEPLOY_FILE = 0x100,
        OPTION_MODEL_FILE,
        OPTION_FORCE_FP32,
        OPTION_NO_PREVIEW,
    };

    static struct option longOptions[] =
    {
        { "deploy", 1, NULL, OPTION_DEPLOY_FILE },
        { "model",  1, NULL, OPTION_MODEL_FILE  },
        { "fp32",   0, NULL, OPTION_FORCE_FP32  },
        { "no-preview", 0, NULL, OPTION_NO_PREVIEW },
        { 0 },
    };

    int c;
    while ((c = getopt_long(argc, argv, "s::v::h", longOptions, NULL)) != -1)
    {
        switch (c)
        {
            case OPTION_DEPLOY_FILE:
                g_deployFile = optarg;
                break;
            case OPTION_MODEL_FILE:
                g_modelFile = optarg;
                break;
            case OPTION_NO_PREVIEW:
                g_bNoPreview = true;
                break;
            case OPTION_FORCE_FP32:
                g_mode = true;
                break;
            case 's':
                g_bProfiling = true;
                break;
            case 'v':
                g_bVerbose = true;
                break;
            default:
                return false;
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    // Parse command line options
    if (!parseCmdline(argc, argv))
    {
        printHelp();
        return 0;
    }

    // Set stdin to non-blocking mode.
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~ICANON;
    ttystate.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);

    // Create EGLRender
    if (!g_bNoPreview)
    {
        g_eglRenderer = NvEglRenderer::createEglRenderer("renderer0", 640, 480, 0, 0);
        if (!g_eglRenderer)
            ORIGINATE_ERROR("Failed to create EGL renderer");
    }

    // Create the CameraProvider object.
    g_cameraProvider.reset(CameraProvider::create());

    std::vector<StreamConsumer*> consumers;

    // Create video encoder consumers
#if 1
    VideoEncodeStreamConsumer consumer1("enc0", "output1.h265", Size2D<uint32_t>(640, 480));
    VideoEncodeStreamConsumer consumer2("enc1", "output2.h265", Size2D<uint32_t>(1280, 720));
    VideoEncodeStreamConsumer consumer3("enc2", "output3.h265", Size2D<uint32_t>(1920, 1080));
    consumers.push_back(&consumer1);
    consumers.push_back(&consumer2);
    consumers.push_back(&consumer3);
#endif

#if ENABLE_TRT
    // Create TRT consumer
    // Get full resolution
    Size2D<uint32_t> fullSize;

    struct sysinfo info;
    if (sysinfo(&info) != 0)
        ORIGINATE_ERROR("Error reading system info");

    // For Jetson Nano (2GB), It is failed to get buffer by EGLStreamProducer for high
    // resolution (3264x2464). So fix resolution size to 1920x1080.
    if (info.totalram < (unsigned long)MEMORY_2GB)
    {
       fullSize.width() = 1920;
       fullSize.height() = 1080;
    }
    else
    {
       getFullResolution(&fullSize);
    }

    TRTStreamConsumer consumer4("trt", "trt.h264", fullSize, g_eglRenderer);
    consumer4.setDeployFile(g_deployFile);
    consumer4.setModelFile(g_modelFile);
    consumer4.setMode(g_mode);
    consumer4.initTRTContext();
    consumers.push_back(&consumer4);
#endif

    // Run EGLStream Producer.
    runArgusProducer(consumers);

    // Destroy CameraProvider
    g_cameraProvider.reset(NULL);

    // Destroy resources
    if (g_eglRenderer)
        delete g_eglRenderer;

    return 0;
}
