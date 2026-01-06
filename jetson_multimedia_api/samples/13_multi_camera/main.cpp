/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
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

#include <Argus/Argus.h>
#include <EGLStream/EGLStream.h>
#include <EGLStream/NV/ImageNativeBuffer.h>

#include <nvbuf_utils.h>
#include <NvEglRenderer.h>

#include <stdio.h>
#include <stdlib.h>

using namespace Argus;
using namespace EGLStream;

/* Constants */
static const uint32_t            MAX_CAMERA_NUM = 6;
static const uint32_t            DEFAULT_FRAME_COUNT = 100;
static const uint32_t            DEFAULT_FPS = 30;
static const Size2D<uint32_t>    STREAM_SIZE(640, 480);

/* Globals */
UniqueObj<CameraProvider>  g_cameraProvider;
NvEglRenderer*             g_renderer = NULL;
uint32_t                   g_stream_num = MAX_CAMERA_NUM;
uint32_t                   g_frame_count = DEFAULT_FRAME_COUNT;

/* Debug print macros */
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)
#define CONSUMER_PRINT(...) printf("CONSUMER: " __VA_ARGS__)

namespace ArgusSamples
{

/* An utility class to hold all resources of one capture session */
class CaptureHolder : public Destructable
{
public:
    explicit CaptureHolder();
    virtual ~CaptureHolder();

    bool initialize(CameraDevice *device);

    CaptureSession* getSession() const
    {
        return m_captureSession.get();
    }

    OutputStream* getStream() const
    {
        return m_outputStream.get();
    }

    Request* getRequest() const
    {
        return m_request.get();
    }

    virtual void destroy()
    {
        delete this;
    }

private:
    UniqueObj<CaptureSession> m_captureSession;
    UniqueObj<OutputStream> m_outputStream;
    UniqueObj<Request> m_request;
};

CaptureHolder::CaptureHolder()
{
}

CaptureHolder::~CaptureHolder()
{
    /* Destroy the output stream */
    m_outputStream.reset();
}

bool CaptureHolder::initialize(CameraDevice *device)
{
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(g_cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to get ICameraProvider interface");

    /* Create the capture session using the first device and get the core interface */
    m_captureSession.reset(iCameraProvider->createCaptureSession(device));
    ICaptureSession *iCaptureSession = interface_cast<ICaptureSession>(m_captureSession);
    IEventProvider *iEventProvider = interface_cast<IEventProvider>(m_captureSession);
    if (!iCaptureSession || !iEventProvider)
        ORIGINATE_ERROR("Failed to create CaptureSession");

    /* Create the OutputStream */
    UniqueObj<OutputStreamSettings> streamSettings(
        iCaptureSession->createOutputStreamSettings(STREAM_TYPE_EGL));
    IEGLOutputStreamSettings *iEglStreamSettings =
        interface_cast<IEGLOutputStreamSettings>(streamSettings);
    if (!iEglStreamSettings)
        ORIGINATE_ERROR("Failed to create EglOutputStreamSettings");

    iEglStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
    iEglStreamSettings->setEGLDisplay(g_renderer->getEGLDisplay());
    iEglStreamSettings->setResolution(STREAM_SIZE);

    m_outputStream.reset(iCaptureSession->createOutputStream(streamSettings.get()));

    /* Create capture request and enable the output stream */
    m_request.reset(iCaptureSession->createRequest());
    IRequest *iRequest = interface_cast<IRequest>(m_request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to create Request");
    iRequest->enableOutputStream(m_outputStream.get());

    ISourceSettings *iSourceSettings =
            interface_cast<ISourceSettings>(iRequest->getSourceSettings());
    if (!iSourceSettings)
        ORIGINATE_ERROR("Failed to get ISourceSettings interface");
    iSourceSettings->setFrameDurationRange(Range<uint64_t>(1e9/DEFAULT_FPS));

    return true;
}


/*
 * Argus Consumer Thread:
 * This is the thread acquires buffers from each stream and composite them to
 * one frame. Finally it renders the composited frame through EGLRenderer.
 */
class ConsumerThread : public Thread
{
public:
    explicit ConsumerThread(std::vector<OutputStream*> &streams) :
        m_streams(streams),
        m_framesRemaining(g_frame_count),
        m_compositedFrame(0)
    {
    }
    virtual ~ConsumerThread();

protected:
    /** @name Thread methods */
    /**@{*/
    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();
    /**@}*/

    std::vector<OutputStream*> &m_streams;
    uint32_t m_framesRemaining;
    UniqueObj<FrameConsumer> m_consumers[MAX_CAMERA_NUM];
    int m_dmabufs[MAX_CAMERA_NUM];
    NvBufferCompositeParams m_compositeParam;
    int m_compositedFrame;
};

ConsumerThread::~ConsumerThread()
{
    if (m_compositedFrame)
        NvBufferDestroy(m_compositedFrame);

    for (uint32_t i = 0; i < m_streams.size(); i++)
        if (m_dmabufs[i])
            NvBufferDestroy(m_dmabufs[i]);
}

bool ConsumerThread::threadInitialize()
{
    NvBufferRect dstCompRect[6];
    int32_t spacing = 10;
    NvBufferCreateParams input_params = {0};

    // Initialize destination composite rectangles
    // The window layout is as below
    // +-------------------------------+-----------------------------------+
    // |                                                                   |
    // |                                    +-------+      +-------+       |
    // |                                    |       |      |       |       |
    // |     Frame 0                        |Frame1 |      |Frame2 |       |
    // |                                    |       |      |       |       |
    // |                                    +-------+      +-------+       |
    // |                                                                   |
    // |                                    +-------+      +-------+       |
    // |                                    |       |      |       |       |
    // |                                    |Frame3 |      |Frame4 |       |
    // |                                    |       |      |       |       |
    // |                                    +-------+      +-------+       |
    // |                                                                   |
    // |                                    +-------+                      |
    // |                                    |       |                      |
    // |                                    |Frame5 |                      |
    // |                                    |       |                      |
    // |                                    +-------+                      |
    // |                                                                   |
    // +-------------------------------+-----------------------------------+
    int32_t cellWidth = (STREAM_SIZE.width() / 2 - spacing * 3) / 2;
    int32_t cellHeight = (STREAM_SIZE.height() - spacing * 4) / 3;

    dstCompRect[0].top  = 0;
    dstCompRect[0].left = 0;
    dstCompRect[0].width = STREAM_SIZE.width();
    dstCompRect[0].height = STREAM_SIZE.height();

    dstCompRect[1].top  = spacing;
    dstCompRect[1].left = STREAM_SIZE.width() / 2 + spacing;
    dstCompRect[1].width = cellWidth;
    dstCompRect[1].height = cellHeight;

    dstCompRect[2].top  = spacing;
    dstCompRect[2].left = STREAM_SIZE.width() / 2 + cellWidth + spacing * 2;
    dstCompRect[2].width = cellWidth;
    dstCompRect[2].height = cellHeight;

    dstCompRect[3].top  = cellHeight + spacing * 2;
    dstCompRect[3].left = STREAM_SIZE.width() / 2 + spacing;
    dstCompRect[3].width = cellWidth;
    dstCompRect[3].height = cellHeight;

    dstCompRect[4].top  = cellHeight + spacing * 2;
    dstCompRect[4].left = STREAM_SIZE.width() / 2 + cellWidth + spacing * 2;
    dstCompRect[4].width = cellWidth;
    dstCompRect[4].height = cellHeight;

    dstCompRect[5].top  = cellHeight * 2 + spacing * 3;
    dstCompRect[5].left = STREAM_SIZE.width() / 2 + spacing;
    dstCompRect[5].width = cellWidth;
    dstCompRect[5].height = cellHeight;

    /* Allocate composited buffer */
    input_params.payloadType = NvBufferPayload_SurfArray;
    input_params.width = STREAM_SIZE.width();
    input_params.height = STREAM_SIZE.height();
    input_params.layout = NvBufferLayout_Pitch;
    input_params.colorFormat = NvBufferColorFormat_NV12;
    input_params.nvbuf_tag = NvBufferTag_VIDEO_CONVERT;

    NvBufferCreateEx (&m_compositedFrame, &input_params);
    if (!m_compositedFrame)
        ORIGINATE_ERROR("Failed to allocate composited buffer");

    /* Initialize composite parameters */
    memset(&m_compositeParam, 0, sizeof(m_compositeParam));
    m_compositeParam.composite_flag = NVBUFFER_COMPOSITE;
    m_compositeParam.input_buf_count = m_streams.size();
    memcpy(m_compositeParam.dst_comp_rect, dstCompRect,
                sizeof(NvBufferRect) * m_compositeParam.input_buf_count);
    for (uint32_t i = 0; i < 6; i++)
    {
        m_compositeParam.dst_comp_rect_alpha[i] = 1.0f;
        m_compositeParam.src_comp_rect[i].top = 0;
        m_compositeParam.src_comp_rect[i].left = 0;
        m_compositeParam.src_comp_rect[i].width = STREAM_SIZE.width();
        m_compositeParam.src_comp_rect[i].height = STREAM_SIZE.height();
    }

    /* Initialize buffer handles. Buffer will be created by FrameConsumer */
    memset(m_dmabufs, 0, sizeof(m_dmabufs));

    /* Create the FrameConsumer */
    for (uint32_t i = 0; i < m_streams.size(); i++)
    {
        m_consumers[i].reset(FrameConsumer::create(m_streams[i]));
    }

    return true;
}

bool ConsumerThread::threadExecute()
{
    IEGLOutputStream *iEglOutputStreams[MAX_CAMERA_NUM];
    IFrameConsumer *iFrameConsumers[MAX_CAMERA_NUM];

    for (uint32_t i = 0; i < m_streams.size(); i++)
    {
        iEglOutputStreams[i] = interface_cast<IEGLOutputStream>(m_streams[i]);
        iFrameConsumers[i] = interface_cast<IFrameConsumer>(m_consumers[i]);
        if (!iFrameConsumers[i])
            ORIGINATE_ERROR("Failed to get IFrameConsumer interface");

        /* Wait until the producer has connected to the stream */
        CONSUMER_PRINT("Waiting until producer is connected...\n");
        if (iEglOutputStreams[i]->waitUntilConnected() != STATUS_OK)
            ORIGINATE_ERROR("Stream failed to connect.");
        CONSUMER_PRINT("Producer has connected; continuing.\n");
    }

    while (m_framesRemaining--)
    {
        for (uint32_t i = 0; i < m_streams.size(); i++)
        {
            /* Acquire a frame */
            UniqueObj<Frame> frame(iFrameConsumers[i]->acquireFrame());
            IFrame *iFrame = interface_cast<IFrame>(frame);
            if (!iFrame)
                break;

            /* Get the IImageNativeBuffer extension interface */
            NV::IImageNativeBuffer *iNativeBuffer =
                interface_cast<NV::IImageNativeBuffer>(iFrame->getImage());
            if (!iNativeBuffer)
                ORIGINATE_ERROR("IImageNativeBuffer not supported by Image.");

            /* If we don't already have a buffer, create one from this image.
               Otherwise, just blit to our buffer */
            if (!m_dmabufs[i])
            {
                m_dmabufs[i] = iNativeBuffer->createNvBuffer(iEglOutputStreams[i]->getResolution(),
                                                          NvBufferColorFormat_YUV420,
                                                          NvBufferLayout_BlockLinear);
                if (!m_dmabufs[i])
                    CONSUMER_PRINT("\tFailed to create NvBuffer\n");
            }
            else if (iNativeBuffer->copyToNvBuffer(m_dmabufs[i]) != STATUS_OK)
            {
                ORIGINATE_ERROR("Failed to copy frame to NvBuffer.");
            }
        }

        CONSUMER_PRINT("Render frame %d\n", g_frame_count - m_framesRemaining);
        if (m_streams.size() > 1)
        {
            /* Composite multiple input to one frame */
            NvBufferComposite(m_dmabufs, m_compositedFrame, &m_compositeParam);
            g_renderer->render(m_compositedFrame);
        }
        else
            g_renderer->render(m_dmabufs[0]);
    }

    CONSUMER_PRINT("Done.\n");

    requestShutdown();

    return true;
}

bool ConsumerThread::threadShutdown()
{
    return true;
}


/*
 * Argus Producer Thread:
 * Open the Argus camera driver and detect how many camera devices available.
 * Create one OutputStream for each camera device. Launch consumer thread
 * and then submit FRAME_COUNT capture requests.
 */
static bool execute()
{
    /* Initialize EGL renderer */
    g_renderer = NvEglRenderer::createEglRenderer("renderer0", STREAM_SIZE.width(),
                                            STREAM_SIZE.height(), 0, 0);
    if (!g_renderer)
        ORIGINATE_ERROR("Failed to create EGLRenderer.");

    /* Initialize the Argus camera provider */
    g_cameraProvider = UniqueObj<CameraProvider>(CameraProvider::create());
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(g_cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to get ICameraProvider interface");
    printf("Argus Version: %s\n", iCameraProvider->getVersion().c_str());

    /* Get the camera devices */
    std::vector<CameraDevice*> cameraDevices;
    iCameraProvider->getCameraDevices(&cameraDevices);
    if (cameraDevices.size() == 0)
        ORIGINATE_ERROR("No cameras available");

    UniqueObj<CaptureHolder> captureHolders[MAX_CAMERA_NUM];
    uint32_t streamCount = cameraDevices.size() < MAX_CAMERA_NUM ?
            cameraDevices.size() : MAX_CAMERA_NUM;
    if (streamCount > g_stream_num)
        streamCount = g_stream_num;
    for (uint32_t i = 0; i < streamCount; i++)
    {
        captureHolders[i].reset(new CaptureHolder);
        if (!captureHolders[i].get()->initialize(cameraDevices[i]))
            ORIGINATE_ERROR("Failed to initialize Camera session %d", i);

    }

    std::vector<OutputStream*> streams;
    for (uint32_t i = 0; i < streamCount; i++)
        streams.push_back(captureHolders[i].get()->getStream());

    /* Start the rendering thread */
    ConsumerThread consumerThread(streams);
    PROPAGATE_ERROR(consumerThread.initialize());
    PROPAGATE_ERROR(consumerThread.waitRunning());

    /* Submit capture requests */
    for (uint32_t i = 0; i < g_frame_count; i++)
    {
        for (uint32_t j = 0; j < streamCount; j++)
        {
            ICaptureSession *iCaptureSession =
                    interface_cast<ICaptureSession>(captureHolders[j].get()->getSession());
            Request *request = captureHolders[j].get()->getRequest();
            uint32_t frameId = iCaptureSession->capture(request);
            if (frameId == 0)
                ORIGINATE_ERROR("Failed to submit capture request");
        }
    }

    /* Wait for idle */
    for (uint32_t i = 0; i < streamCount; i++)
    {
        ICaptureSession *iCaptureSession =
            interface_cast<ICaptureSession>(captureHolders[i].get()->getSession());
        iCaptureSession->waitForIdle();
    }

    /* Destroy the capture resources */
    for (uint32_t i = 0; i < streamCount; i++)
    {
        captureHolders[i].reset();
    }

    /* Wait for the rendering thread to complete */
    PROPAGATE_ERROR(consumerThread.shutdown());

    /* Shut down Argus */
    g_cameraProvider.reset();

    /* Cleanup EGL Renderer */
    delete g_renderer;

    return true;
}

}; /* namespace ArgusSamples */

static void printHelp()
{
    printf("Usage: multi_camera [OPTIONS]\n"
           "Options:\n"
           "  -n <num>      Max number of output streams (1 to 6)\n"
           "  -c <count>    Total frame count\n"
           "  -h            Print this help\n");
}

static bool parseCmdline(int argc, char * argv[])
{
    int c;
    while ((c = getopt(argc, argv, "n:c:h")) != -1)
    {
        switch (c)
        {
            case 'n':
                g_stream_num = atoi(optarg);
                if (g_stream_num < 1 || g_stream_num > MAX_CAMERA_NUM)
                {
                    printf("Invalid number of streams\n");
                    return false;
                }
                break;
            case 'c':
                g_frame_count = atoi(optarg);
                if (g_frame_count < 1)
                {
                    printf("Invalid frame count\n");
                    return false;
                }
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

    if (!ArgusSamples::execute())
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
