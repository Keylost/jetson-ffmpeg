/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION. All rights reserved.
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

#include "Error.h"
#include "Thread.h"

#include <NvEglRenderer.h>
#include <NvJpegEncoder.h>

#include <Argus/Argus.h>
#include <EGLStream/EGLStream.h>
#include <EGLStream/NV/ImageNativeBuffer.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <iostream>
#include <fstream>

using namespace Argus;
using namespace EGLStream;

/* Configurations below can be overrided by cmdline */
static uint32_t CAPTURE_TIME  = 1; /* In seconds. */
static int      CAPTURE_FPS   = 30;
static uint32_t SENSOR_MODE   = 0;
static Size2D<uint32_t> PREVIEW_SIZE (640, 480);
static Size2D<uint32_t> CAPTURE_SIZE (1920, 1080);
static bool    DO_STAT = false;
static bool    VERBOSE_ENABLE = false;
static bool    DO_JPEG_ENCODE = true;
static const uint64_t WAIT_FOR_EVENT_TIMEOUT  = 1500000000;
static const uint64_t ACQUIRE_FRAME_TIMEOUT   = 3000000000;

#define JPEG_BUFFER_SIZE    (CAPTURE_SIZE.area() * 3 / 2)

/* Debug print macros. */
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)
#define CONSUMER_PRINT(...) printf("CONSUMER: " __VA_ARGS__)

namespace ArgusSamples
{

/*******************************************************************************
 * Base Consumer thread:
 *   Creates an EGLStream::FrameConsumer object to read frames from the
 *   OutputStream, then creates/populates an NvBuffer (dmabuf) from the frames
 *   to be processed by processV4L2Fd.
 ******************************************************************************/
class ConsumerThread : public Thread
{
public:
    explicit ConsumerThread(OutputStream* stream, IEventProvider *eventProvider) :
        m_stream(stream),
        m_dmabuf(-1)
    {
        m_eventProvider = eventProvider;
        errorStatus = false;
    }
    bool errorStatus;
    virtual ~ConsumerThread();

protected:
    /** @name Thread methods */
    /**@{*/
    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();
    /**@}*/

    virtual bool processV4L2Fd(int32_t fd, uint64_t frameNumber) = 0;

    OutputStream* m_stream;
    UniqueObj<FrameConsumer> m_consumer;
    int m_dmabuf;

    IEventProvider* m_eventProvider;
    UniqueObj<EventQueue> m_queue;

private:
    void printErrorStatus(Argus::Status status);
};

void ConsumerThread::printErrorStatus(Argus::Status status)
{
    switch (status)
    {
        case Argus::STATUS_OK:
            CONSUMER_PRINT("Argus::STATUS_OK \n");
            break;
        case Argus::STATUS_INVALID_PARAMS:
            CONSUMER_PRINT("Argus::STATUS_INVALID_PARAMS \n");
            break;
        case Argus::STATUS_INVALID_SETTINGS:
            CONSUMER_PRINT("Argus::STATUS_INVALID_SETTINGS \n");
            break;
        case Argus::STATUS_UNAVAILABLE:
            CONSUMER_PRINT("Argus::STATUS_UNAVAILABLE \n");
            break;
        case Argus::STATUS_OUT_OF_MEMORY:
            CONSUMER_PRINT("Argus::STATUS_OUT_OF_MEMORY \n");
            break;
        case Argus::STATUS_UNIMPLEMENTED:
            CONSUMER_PRINT("Argus::STATUS_UNIMPLEMENTED \n");
            break;
        case Argus::STATUS_TIMEOUT:
            CONSUMER_PRINT("Argus::STATUS_TIMEOUT \n");
            break;
        case Argus::STATUS_CANCELLED:
            CONSUMER_PRINT("Argus::STATUS_CANCELLED \n");
            break;
        case Argus::STATUS_DISCONNECTED:
            CONSUMER_PRINT("Argus::STATUS_DISCONNECTED \n");
            break;
        case Argus::STATUS_END_OF_STREAM:
            CONSUMER_PRINT("Argus::STATUS_END_OF_STREAM \n");
            break;
        default:
            CONSUMER_PRINT("BAD STATUS \n");
            break;
    }
    return;
}

ConsumerThread::~ConsumerThread()
{
    if (m_dmabuf != -1)
        NvBufferDestroy(m_dmabuf);
}

bool ConsumerThread::threadInitialize()
{
    std::vector<EventType> eventTypes;
    eventTypes.push_back(EVENT_TYPE_CAPTURE_COMPLETE);
    eventTypes.push_back(EVENT_TYPE_ERROR);
    eventTypes.push_back(EVENT_TYPE_CAPTURE_STARTED);
    m_queue = UniqueObj<EventQueue>(m_eventProvider->createEventQueue(eventTypes));

    /* Create the FrameConsumer. */
    m_consumer = UniqueObj<FrameConsumer>(FrameConsumer::create(m_stream));
    if (!m_consumer)
        ORIGINATE_ERROR("Failed to create FrameConsumer");

    return true;
}

bool ConsumerThread::threadExecute()
{
    IEGLOutputStream *iEglOutputStream = interface_cast<IEGLOutputStream>(m_stream);
    IFrameConsumer *iFrameConsumer = interface_cast<IFrameConsumer>(m_consumer);
    IEventQueue *iQueue = interface_cast<IEventQueue>(m_queue);

    /* Wait until the producer has connected to the stream. */
    CONSUMER_PRINT("Waiting until producer is connected...\n");
    if (iEglOutputStream->waitUntilConnected() != STATUS_OK)
        ORIGINATE_ERROR("Stream failed to connect.");
    CONSUMER_PRINT("Producer has connected; continuing.\n");

    while (true)
    {
        Argus::Status argusStatus;
        m_eventProvider->waitForEvents(m_queue.get(), WAIT_FOR_EVENT_TIMEOUT);
        if (iQueue->getSize() == 0)
            break;

        const Event* event = iQueue->getEvent(iQueue->getSize() - 1);
        const IEvent* iEvent = interface_cast<const IEvent>(event);
        if (!iEvent)
            ORIGINATE_ERROR("Failed to get IEvent interface \n");

        if (iEvent->getEventType() == EVENT_TYPE_ERROR)
        {
            const IEventError* iEventError = interface_cast<const IEventError>(event);
            argusStatus = iEventError->getStatus();
            printErrorStatus(argusStatus);
            errorStatus = true;
            break;
        }

        /* Acquire a frame. */
        UniqueObj<Frame> frame(iFrameConsumer->acquireFrame(ACQUIRE_FRAME_TIMEOUT, &argusStatus));
        if (argusStatus != Argus::STATUS_OK)
        {
            printErrorStatus(argusStatus);
            errorStatus = true;
            break;
        }
        IFrame *iFrame = interface_cast<IFrame>(frame);
        if (!iFrame)
            break;

        /* Get the IImageNativeBuffer extension interface. */
        NV::IImageNativeBuffer *iNativeBuffer =
            interface_cast<NV::IImageNativeBuffer>(iFrame->getImage());
        if (!iNativeBuffer)
            ORIGINATE_ERROR("IImageNativeBuffer not supported by Image.");

        /* If we don't already have a buffer, create one from this image.
           Otherwise, just blit to our buffer. */
        if (m_dmabuf == -1)
        {
            m_dmabuf = iNativeBuffer->createNvBuffer(iEglOutputStream->getResolution(),
                                                     NvBufferColorFormat_YUV420,
                                                     NvBufferLayout_BlockLinear);
            if (m_dmabuf == -1)
                CONSUMER_PRINT("\tFailed to create NvBuffer\n");
        }
        else if (iNativeBuffer->copyToNvBuffer(m_dmabuf) != STATUS_OK)
        {
            ORIGINATE_ERROR("Failed to copy frame to NvBuffer.");
        }

        /* Process frame. */
        processV4L2Fd(m_dmabuf, iFrame->getNumber());
    }

    CONSUMER_PRINT("Done.\n");

    requestShutdown();

    return true;
}

bool ConsumerThread::threadShutdown()
{
    return true;
}

/*******************************************************************************
 * Preview Consumer thread:
 *   Read frames from the OutputStream and render it on display.
 ******************************************************************************/
class PreviewConsumerThread : public ConsumerThread
{
public:
    PreviewConsumerThread(OutputStream *stream, IEventProvider *eventProvider, NvEglRenderer *renderer);
    ~PreviewConsumerThread();

private:
    bool threadInitialize();
    bool threadShutdown();
    bool processV4L2Fd(int32_t fd, uint64_t frameNumber);

    NvEglRenderer *m_renderer;
};

PreviewConsumerThread::PreviewConsumerThread(OutputStream *stream,
                                             IEventProvider *eventProvider,
                                             NvEglRenderer *renderer) :
    ConsumerThread(stream, eventProvider),
    m_renderer(renderer)
{
}

PreviewConsumerThread::~PreviewConsumerThread()
{
}

bool PreviewConsumerThread::threadInitialize()
{
    if (!ConsumerThread::threadInitialize())
        return false;

    if (DO_STAT)
        m_renderer->enableProfiling();

    return true;
}

bool PreviewConsumerThread::threadShutdown()
{
    if (DO_STAT)
        m_renderer->printProfilingStats();

    return ConsumerThread::threadShutdown();
}

bool PreviewConsumerThread::processV4L2Fd(int32_t fd, uint64_t frameNumber)
{
    m_renderer->render(fd);
    return true;
}

/*******************************************************************************
 * Capture Consumer thread:
 *   Read frames from the OutputStream and save it to JPEG file.
 ******************************************************************************/
class CaptureConsumerThread : public ConsumerThread
{
public:
    CaptureConsumerThread(OutputStream *stream, IEventProvider *eventProvider);
    ~CaptureConsumerThread();

private:
    bool threadInitialize();
    bool threadShutdown();
    bool processV4L2Fd(int32_t fd, uint64_t frameNumber);

    NvJPEGEncoder *m_JpegEncoder;
    unsigned char *m_OutputBuffer;
};

CaptureConsumerThread::CaptureConsumerThread(OutputStream *stream, IEventProvider *eventProvider) :
    ConsumerThread(stream, eventProvider),
    m_JpegEncoder(NULL),
    m_OutputBuffer(NULL)
{
}

CaptureConsumerThread::~CaptureConsumerThread()
{
    if (m_JpegEncoder)
        delete m_JpegEncoder;

    if (m_OutputBuffer)
        delete [] m_OutputBuffer;
}

bool CaptureConsumerThread::threadInitialize()
{
    if (!ConsumerThread::threadInitialize())
        return false;

    m_OutputBuffer = new unsigned char[JPEG_BUFFER_SIZE];
    if (!m_OutputBuffer)
        return false;

    m_JpegEncoder = NvJPEGEncoder::createJPEGEncoder("jpenenc");
    if (!m_JpegEncoder)
        ORIGINATE_ERROR("Failed to create JPEGEncoder.");

    if (DO_STAT)
        m_JpegEncoder->enableProfiling();

    return true;
}

bool CaptureConsumerThread::threadShutdown()
{
    if (DO_STAT)
        m_JpegEncoder->printProfilingStats();

    return ConsumerThread::threadShutdown();
}

bool CaptureConsumerThread::processV4L2Fd(int32_t fd, uint64_t frameNumber)
{
    char filename[FILENAME_MAX];
    sprintf(filename, "output%03u.jpg", (unsigned) frameNumber);

    std::ofstream *outputFile = new std::ofstream(filename);
    if (outputFile)
    {
        unsigned long size = JPEG_BUFFER_SIZE;
        unsigned char *buffer = m_OutputBuffer;
        m_JpegEncoder->encodeFromFd(fd, JCS_YCbCr, &buffer, size);
        outputFile->write((char *)buffer, size);
        delete outputFile;
    }

    return true;
}

/**
 * Argus Producer thread:
 *   Opens the Argus camera driver, creates two OutputStreams to output to
 *   Preview Consumer and Capture Consumer respectively, then performs repeating
 *   capture requests for CAPTURE_TIME seconds before closing the producer and
 *   Argus driver.
 *
 * @param renderer     : render handler for camera preview
 */
static bool execute(NvEglRenderer *renderer)
{
    UniqueObj<OutputStream> captureStream;
    CaptureConsumerThread *captureConsumerThread = NULL;

    /* Create the CameraProvider object and get the core interface */
    UniqueObj<CameraProvider> cameraProvider = UniqueObj<CameraProvider>(CameraProvider::create());
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to create CameraProvider");

    /* Get the camera devices */
    std::vector<CameraDevice*> cameraDevices;
    iCameraProvider->getCameraDevices(&cameraDevices);
    if (cameraDevices.size() == 0)
        ORIGINATE_ERROR("No cameras available");

    ICameraProperties *iCameraProperties = interface_cast<ICameraProperties>(cameraDevices[0]);
    if (!iCameraProperties)
        ORIGINATE_ERROR("Failed to get ICameraProperties interface");

    /* Create the capture session using the first device and get the core interface */
    UniqueObj<CaptureSession> captureSession(
            iCameraProvider->createCaptureSession(cameraDevices[0]));
    ICaptureSession *iCaptureSession = interface_cast<ICaptureSession>(captureSession);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to get ICaptureSession interface");

    IEventProvider *iEventProvider = interface_cast<IEventProvider>(captureSession);

    /* Initiaialize the settings of output stream */
    PRODUCER_PRINT("Creating output stream\n");
    UniqueObj<OutputStreamSettings> streamSettings(
        iCaptureSession->createOutputStreamSettings(STREAM_TYPE_EGL));
    IEGLOutputStreamSettings *iEglStreamSettings =
        interface_cast<IEGLOutputStreamSettings>(streamSettings);
    if (!iEglStreamSettings)
        ORIGINATE_ERROR("Failed to get IEGLOutputStreamSettings interface");

    iEglStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
    iEglStreamSettings->setEGLDisplay(renderer->getEGLDisplay());
    iEglStreamSettings->setResolution(PREVIEW_SIZE);

    /* Based on above streamSettings, create the preview stream,
       and capture stream if JPEG Encode is required */
    UniqueObj<OutputStream> previewStream(iCaptureSession->createOutputStream(streamSettings.get()));
    if (DO_JPEG_ENCODE) {
        iEglStreamSettings->setResolution(CAPTURE_SIZE);
        captureStream = (UniqueObj<OutputStream>)iCaptureSession->createOutputStream(streamSettings.get());
    }

    /* Launch the FrameConsumer thread to consume frames from the OutputStream */
    PRODUCER_PRINT("Launching consumer thread\n");
    PreviewConsumerThread previewConsumerThread(previewStream.get(), iEventProvider, renderer);
    PROPAGATE_ERROR(previewConsumerThread.initialize());
    if (DO_JPEG_ENCODE) {
        captureConsumerThread = new CaptureConsumerThread(captureStream.get(), iEventProvider);
        PROPAGATE_ERROR(captureConsumerThread->initialize());
    }

    /* Wait until the consumer thread is connected to the stream */
    PROPAGATE_ERROR(previewConsumerThread.waitRunning());
    if (DO_JPEG_ENCODE)
        PROPAGATE_ERROR(captureConsumerThread->waitRunning());

    /* Create capture request and enable its output stream */
    UniqueObj<Request> request(iCaptureSession->createRequest());
    IRequest *iRequest = interface_cast<IRequest>(request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to create Request");
    iRequest->enableOutputStream(previewStream.get());
    if (DO_JPEG_ENCODE)
        iRequest->enableOutputStream(captureStream.get());

    ISensorMode *iSensorMode;
    std::vector<SensorMode*> sensorModes;
    iCameraProperties->getBasicSensorModes(&sensorModes);
    if (sensorModes.size() == 0)
        ORIGINATE_ERROR("Failed to get sensor modes");

    PRODUCER_PRINT("Available Sensor modes :\n");
    for (uint32_t i = 0; i < sensorModes.size(); i++) {
        iSensorMode = interface_cast<ISensorMode>(sensorModes[i]);
        Size2D<uint32_t> resolution = iSensorMode->getResolution();
        PRODUCER_PRINT("[%u] W=%u H=%u\n", i, resolution.width(), resolution.height());
    }

    ISourceSettings *iSourceSettings = interface_cast<ISourceSettings>(iRequest->getSourceSettings());
    if (!iSourceSettings)
        ORIGINATE_ERROR("Failed to get ISourceSettings interface");

    /* Check and set sensor mode */
    if (SENSOR_MODE >= sensorModes.size())
        ORIGINATE_ERROR("Sensor mode index is out of range");
    SensorMode *sensorMode = sensorModes[SENSOR_MODE];
    iSensorMode = interface_cast<ISensorMode>(sensorMode);
    iSourceSettings->setSensorMode(sensorMode);

    /* Check fps */
    Range<uint64_t> sensorDuration(iSensorMode->getFrameDurationRange());
    Range<uint64_t> desireDuration(1e9/CAPTURE_FPS+0.9);
    if (desireDuration.min() < sensorDuration.min() ||
            desireDuration.max() > sensorDuration.max()) {
        PRODUCER_PRINT("Requested FPS out of range. Fall back to 30\n");
        CAPTURE_FPS = 30;
    }
    /* Set the fps */
    iSourceSettings->setFrameDurationRange(Range<uint64_t>(1e9/CAPTURE_FPS));
    renderer->setFPS((float)CAPTURE_FPS);

    /* Submit capture requests. */
    PRODUCER_PRINT("Starting repeat capture requests.\n");
    if (iCaptureSession->repeat(request.get()) != STATUS_OK)
        ORIGINATE_ERROR("Failed to start repeat capture request");

    /* Wait for CAPTURE_TIME seconds. */
    for (uint32_t i = 0; i < CAPTURE_TIME; i++)
    {
        sleep(1);
        if (previewConsumerThread.errorStatus ||
            (DO_JPEG_ENCODE &&
            captureConsumerThread->errorStatus))
            break;
    }

    /* Stop the repeating request and wait for idle. */
    iCaptureSession->stopRepeat();
    iCaptureSession->waitForIdle();

    /* Destroy the output stream to end the consumer thread. */
    previewStream.reset();
    if (DO_JPEG_ENCODE)
        captureStream.reset();

    /* Wait for the consumer thread to complete. */
    PROPAGATE_ERROR(previewConsumerThread.shutdown());
    if (DO_JPEG_ENCODE) {
        PROPAGATE_ERROR(captureConsumerThread->shutdown());
        delete captureConsumerThread;
    }

    PRODUCER_PRINT("Done -- exiting.\n");

    return true;
}

}; /* namespace ArgusSamples */

static void printHelp()
{
    printf("Usage: camera_jpeg_capture [OPTIONS]\n"
           "Options:\n"
           "  --pre-res     Preview resolution WxH [Default 640x480]\n"
           "  --img-res     Capture resolution WxH [Default 1920x1080]\n"
           "  --cap-time    Capture time in sec    [Default 1]\n"
           "  --fps         Frame per second       [Default 30]\n"
           "  --sensor-mode Sensor mode            [Default 0]\n"
           "  --disable-jpg Disable JPEG encode    [Default Enable]\n"
           "  -s            Enable profiling\n"
           "  -v            Enable verbose message\n"
           "  -h            Print this help\n");
}

static bool parseCmdline(int argc, char * argv[])
{
    enum
    {
        OPTION_PREVIEW_RESOLUTION = 0x100,
        OPTION_CAPTURE_RESOLUTION,
        OPTION_CAPTURE_TIME,
        OPTION_FPS,
        OPTION_SENSOR_MODE,
        OPTION_DISABLE_JPEG_ENCODE,
    };

    static struct option longOptions[] =
    {
        { "pre-res",     1, NULL, OPTION_PREVIEW_RESOLUTION },
        { "img-res",     1, NULL, OPTION_CAPTURE_RESOLUTION },
        { "cap-time",    1, NULL, OPTION_CAPTURE_TIME },
        { "fps",         1, NULL, OPTION_FPS },
        { "sensor-mode", 1, NULL, OPTION_SENSOR_MODE },
        { "disable-jpg", 0, NULL, OPTION_DISABLE_JPEG_ENCODE },
        { 0 },
    };

    int c, w, h;
    uint32_t t;
    while ((c = getopt_long(argc, argv, "s::v::h", longOptions, NULL)) != -1)
    {
        switch (c)
        {
            case OPTION_PREVIEW_RESOLUTION:
                if (sscanf(optarg, "%dx%d", &w, &h) != 2)
                    return false;
                PREVIEW_SIZE.width() = w;
                PREVIEW_SIZE.height() = h;
                break;
            case OPTION_CAPTURE_RESOLUTION:
                if (sscanf(optarg, "%dx%d", &w, &h) != 2)
                    return false;
                CAPTURE_SIZE.width() = w;
                CAPTURE_SIZE.height() = h;
                break;
            case OPTION_CAPTURE_TIME:
                if (sscanf(optarg, "%d", &t) != 1)
                    return false;
                CAPTURE_TIME = t;
                break;
            case OPTION_FPS:
                if (sscanf(optarg, "%d", &w) != 1)
                    return false;
                CAPTURE_FPS = w;
                break;
            case OPTION_SENSOR_MODE:
                if (sscanf(optarg, "%d", &t) != 1)
                    return false;
                SENSOR_MODE = t;
                break;
            case OPTION_DISABLE_JPEG_ENCODE:
                DO_JPEG_ENCODE = false;
                break;
            case 's':
                DO_STAT = true;
                break;
            case 'v':
                VERBOSE_ENABLE = true;
                break;
            default:
                return false;
        }
    }
    return true;
}

int main(int argc, char * argv[])
{
    if (!parseCmdline(argc, argv))
    {
        printHelp();
        return EXIT_FAILURE;
    }

    NvEglRenderer *renderer = NvEglRenderer::createEglRenderer("renderer0", PREVIEW_SIZE.width(),
                                            PREVIEW_SIZE.height(), 0, 0);
    if (!renderer)
        ORIGINATE_ERROR("Failed to create EGLRenderer.");

    if (!ArgusSamples::execute(renderer))
        return EXIT_FAILURE;

    delete renderer;

    return EXIT_SUCCESS;
}
