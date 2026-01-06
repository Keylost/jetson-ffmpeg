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

#include <fstream>
#include <iostream>
#include <assert.h>
#include <opencv2/core/core.hpp>
#include "TRTStreamConsumer.h"
#include <EGLStream/NV/ImageNativeBuffer.h>
#include "Error.h"
#include "NvCudaProc.h"
#include "NvEglRenderer.h"

#define TIMESPEC_DIFF_USEC(timespec1, timespec2) \
    (((timespec1)->tv_sec - (timespec2)->tv_sec) * 1000000L + \
        (timespec1)->tv_usec - (timespec2)->tv_usec)

#define IS_EOS_BUFFER(buf)  (buf.fd < 0)
#define MAX_QUEUE_SIZE      (10)
#define MAX_TRT_BUFFER      (10)
#define TRT_INTERVAL        (1)

#define TRT_MODEL    GOOGLENET_THREE_CLASS

extern bool g_bVerbose;
extern bool g_bProfiling;

TRTStreamConsumer::TRTStreamConsumer(const char *name, const char *outputFilename,
        Size2D<uint32_t> size, NvEglRenderer *renderer, bool hasEncoding) :
    StreamConsumer(name, size),
    m_VideoEncoder(name, outputFilename, size.width(), size.height(), V4L2_PIX_FMT_H264),
    m_hasEncoding(hasEncoding),
    m_eglRenderer(renderer)
{
    nvosd_context = nvosd_create_context();
    m_VideoEncoder.setBufferDoneCallback(bufferDoneCallback, this);
    m_mode = false;
}

TRTStreamConsumer::~TRTStreamConsumer()
{
    nvosd_destroy_context(nvosd_context);
}

void TRTStreamConsumer::initTRTContext()
{
    // Create TRT model
    Log("Creating TRT model..\n");
    m_TRTContext.setModelIndex(TRT_MODEL);
    m_TRTContext.setMode(m_mode);
    m_TRTContext.buildTrtContext(m_deployFile, m_modelFile);
    m_TRTContext.setTrtProfilerEnabled(true);
    Log("Batch size: %d\n", m_TRTContext.getBatchSize());
}

bool TRTStreamConsumer::threadInitialize()
{
    NvBufferCreateParams input_params = {0};

    if (!StreamConsumer::threadInitialize())
        return false;

    // Init encoder
    if (m_hasEncoding)
        m_VideoEncoder.initialize();

    // Check if we have enough buffer for batch
    if (TRT_INTERVAL * m_TRTContext.getBatchSize() > MAX_QUEUE_SIZE)
        ORIGINATE_ERROR("TRT_INTERVAL(%d) * BATCH_SIZE must less or equal to QUEUE_SIZE(%d)",
                TRT_INTERVAL, MAX_QUEUE_SIZE);

    input_params.payloadType = NvBufferPayload_SurfArray;
    input_params.nvbuf_tag = NvBufferTag_NONE;
    // Create buffers
    for (unsigned i = 0; i < MAX_QUEUE_SIZE; i++)
    {
        int dmabuf_fd;
        input_params.width = m_size.width();
        input_params.height = m_size.height();
        input_params.layout = NvBufferLayout_BlockLinear;
        input_params.colorFormat = NvBufferColorFormat_YUV420;

        if (NvBufferCreateEx(&dmabuf_fd, &input_params) < 0)
            ORIGINATE_ERROR("Failed to create NvBuffer.");

        m_emptyBufferQueue.push(dmabuf_fd);
    }

    // Create TRT buffers
    for (unsigned i = 0; i < MAX_TRT_BUFFER; i++)
    {
        int fd;
        input_params.width = m_TRTContext.getNetWidth();
        input_params.height = m_TRTContext.getNetHeight();
        input_params.layout = NvBufferLayout_Pitch;
        input_params.colorFormat = NvBufferColorFormat_ABGR32;

        if (NvBufferCreateEx(&fd, &input_params) < 0)
            ORIGINATE_ERROR("Failed to create NvBuffer.");

        m_emptyTRTBufferQueue.push(fd);
    }

    // Launch render and TRT threads
    pthread_create(&m_renderThread, NULL, RenderThreadProc, this);
    pthread_setname_np(m_renderThread,"RendererThread");
    pthread_create(&m_trtThread, NULL, TRTThreadProc, this);
    pthread_setname_np(m_trtThread,"TRTThread");

    return true;
}

bool TRTStreamConsumer::processFrame(Frame *frame)
{
    IFrame *iFrame = interface_cast<IFrame>(frame);
    if (!iFrame)
    {
        static BufferInfo eosBuffer = { -1 };   // EOS
        m_trtBufferQueue.push(eosBuffer);
        m_renderBufferQueue.push(eosBuffer);
        return false;
    }

    BufferInfo buf;
    buf.fd = m_emptyBufferQueue.pop();
    buf.number = iFrame->getNumber();

    // Get the IImageNativeBuffer extension interface and create the fd.
    NV::IImageNativeBuffer *iNativeBuffer =
        interface_cast<NV::IImageNativeBuffer>(iFrame->getImage());
    if (!iNativeBuffer)
        ORIGINATE_ERROR("IImageNativeBuffer not supported by Image.");

    iNativeBuffer->copyToNvBuffer(buf.fd);

    // Do TRT inference every 10 frames
    if (iFrame->getNumber() % TRT_INTERVAL == 0)
    {
        BufferInfo trtBuf;
        trtBuf.fd = m_emptyTRTBufferQueue.pop();
        trtBuf.number = iFrame->getNumber();
        iNativeBuffer->copyToNvBuffer(trtBuf.fd);
        m_trtBufferQueue.push(trtBuf);
    }

    m_renderBufferQueue.push(buf);

    return true;
}

bool TRTStreamConsumer::threadShutdown()
{
    pthread_join(m_renderThread, NULL);
    pthread_join(m_trtThread, NULL);

    if (m_hasEncoding)
        m_VideoEncoder.shutdown();

    // Ensure all buffers are returned by encoder
    assert(m_emptyBufferQueue.size() == MAX_QUEUE_SIZE);

    m_TRTContext.destroyTrtContext();

    // Destroy all buffers
    while (m_emptyBufferQueue.size())
        NvBufferDestroy(m_emptyBufferQueue.pop());

    while (m_emptyTRTBufferQueue.size())
        NvBufferDestroy(m_emptyTRTBufferQueue.pop());

    return StreamConsumer::threadShutdown();
}

bool TRTStreamConsumer::RenderThreadProc()
{
    Log("Render thread started.\n");

    // Start profiling
    if (m_eglRenderer)
        m_eglRenderer->enableProfiling();

    while (true)
    {
        BufferInfo buf = m_renderBufferQueue.pop();

        if (!IS_EOS_BUFFER(buf))
        {
            if (buf.number % TRT_INTERVAL == 0)
            {
                m_rectParams.clear();

                // Get bound box info from TRT thread
                for (int class_num = 0; class_num < m_TRTContext.getModelClassCnt(); class_num++)
                {
                    vector<Rect2f> *bbox = m_bboxesQueue[class_num].pop();

                    if (bbox)   // bbox = NULL means TRT thread has exited
                    {
                        for (unsigned i = 0; i < bbox->size(); i++)
                        {
                            Rect2f &rect = bbox->at(i);
                            NvOSD_RectParams rectParam = { 0 };
                            rectParam.left   = m_size.width()  * rect.x;
                            rectParam.top    = m_size.height() * rect.y;
                            rectParam.width  = m_size.width()  * rect.width;
                            rectParam.height = m_size.height() * rect.height;
                            rectParam.border_width = 5;
                            rectParam.border_color.red = ((class_num == 0) ? 1.0f : 0.0);
                            rectParam.border_color.green = ((class_num == 1) ? 1.0f : 0.0);
                            rectParam.border_color.blue = ((class_num == 2) ? 1.0f : 0.0);
                            m_rectParams.push_back(rectParam);
                        }
                        delete bbox;
                    }
                }
            }

            if (g_bVerbose)
                Log("Render: processing frame %d\n", buf.number);

            // Draw bounding box
            nvosd_draw_rectangles(nvosd_context, MODE_HW, buf.fd,
                        m_rectParams.size(), m_rectParams.data());

            // Do rendering
            if (m_eglRenderer)
            {
                if (buf.number % 30 == 0)
                {
                    NvElementProfiler::NvElementProfilerData data;
                    m_eglRenderer->getProfilingData(data);

                    uint64_t framesCount = data.total_processed_units -
                        m_profilerData.total_processed_units;
                    uint64_t timeElapsed = TIMESPEC_DIFF_USEC(&data.profiling_time,
                            &m_profilerData.profiling_time);
                    m_fps = (float)framesCount * 1e6 / timeElapsed;
                    memcpy(&m_profilerData, &data, sizeof(data));
                    Log("FPS: %f\n", m_fps);
                }

                char overlay[256];
                snprintf(overlay, sizeof(overlay), "Frame %u, FPS %f", buf.number, m_fps);
                m_eglRenderer->setOverlayText(overlay, 10, 30);
                m_eglRenderer->render(buf.fd);
            }
        }

        // Do encoding
        if (m_hasEncoding)
            m_VideoEncoder.encodeFromFd(buf.fd);
        else if (!IS_EOS_BUFFER(buf))
            bufferDoneCallback(buf.fd);

        if (IS_EOS_BUFFER(buf))
            break;
    }

    // Print profiling stats
    if (m_eglRenderer)
        m_eglRenderer->printProfilingStats();

    Log("Render thread exited.\n");
    return true;
}

bool TRTStreamConsumer::TRTThreadProc()
{
    IEGLOutputStream *iEglOutputStream = interface_cast<IEGLOutputStream>(m_stream);
    EGLDisplay display = iEglOutputStream->getEGLDisplay();
    Log("TRT thread started.\n");

    unsigned bufNumInBatch = 0;
    int class_num = 0;
    int classCnt = m_TRTContext.getModelClassCnt();

    while (true)
    {
        BufferInfo buf = m_trtBufferQueue.pop();
        if (IS_EOS_BUFFER(buf))
            break;

        if (g_bVerbose)
            Log("TRT: Add frame %d to batch (%d/%d)\n", buf.number, bufNumInBatch,
                    m_TRTContext.getBatchSize());

        EGLImageKHR eglImage = NvEGLImageFromFd(display, buf.fd);
        size_t batchOffset = bufNumInBatch * m_TRTContext.getNetWidth() *
            m_TRTContext.getNetHeight() * m_TRTContext.getChannel();
        mapEGLImage2Float(&eglImage,
                m_TRTContext.getNetWidth(),
                m_TRTContext.getNetHeight(),
                (TRT_MODEL == GOOGLENET_THREE_CLASS) ? COLOR_FORMAT_BGR : COLOR_FORMAT_RGB,
                (char*) m_TRTContext.getBuffer(0) + batchOffset * sizeof(float),
                m_TRTContext.getOffsets(),
                m_TRTContext.getScales());
        NvDestroyEGLImage(display, eglImage);
        m_emptyTRTBufferQueue.push(buf.fd);

        if (++bufNumInBatch < m_TRTContext.getBatchSize())
            continue;       // Batch not ready, wait for new buffers

        // Inference
        queue<vector<cv::Rect>> rectList_queue[classCnt];
        m_TRTContext.doInference(rectList_queue);

        for (int i = 0; i < classCnt; i++)
        {
            assert(rectList_queue[i].size() == m_TRTContext.getBatchSize());
        }

        for (class_num = 0; class_num < classCnt; class_num++)
        {
            for ( ; !rectList_queue[class_num].empty(); rectList_queue[class_num].pop())
            {
                vector<cv::Rect> &rectList = rectList_queue[class_num].front();
                vector<Rect2f> *bbox = new vector<Rect2f>();

                // Calculate normalized bound box
                for (vector<cv::Rect>::iterator it = rectList.begin(); it != rectList.end(); it++)
                {
                    cv::Rect rect = *it;
                    Rect2f rect2f;
                    rect2f.x      = (float)rect.x      / m_TRTContext.getNetWidth();
                    rect2f.y      = (float)rect.y      / m_TRTContext.getNetHeight();
                    rect2f.width  = (float)rect.width  / m_TRTContext.getNetWidth();
                    rect2f.height = (float)rect.height / m_TRTContext.getNetHeight();

                    bbox->push_back(rect2f);
                }
                m_bboxesQueue[class_num].push(bbox);
            }
        }

        if (g_bVerbose)
            Log("TRT: Batch done (%d/%d)\n", bufNumInBatch, m_TRTContext.getBatchSize());

        bufNumInBatch = 0;  // Reset counter for next batch
    }

    // Tell render thread we are exiting.
    for (class_num = 0; class_num < classCnt; class_num++)
    {
        for (uint32_t i = 0; i < m_TRTContext.getBatchSize(); i++)
            m_bboxesQueue[class_num].push(NULL);
    }

    Log("TRT thread exited.\n");

    return true;
}

void TRTStreamConsumer::bufferDoneCallback(int dmabuf_fd)
{
    m_emptyBufferQueue.push(dmabuf_fd);
}
