/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION. All rights reserved.
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

#include <string.h>
#include <assert.h>
#include "VideoEncoder.h"
#include "Error.h"

#define CHECK_ERROR(expr) \
    do { \
        if ((expr) < 0) { \
            m_VideoEncoder->abort(); \
            ORIGINATE_ERROR(#expr " failed"); \
        } \
    } while (0);

// WAR: Since dqBuffer only happens when a new qBuffer is required, old buffer
// will not be released until new buffer comes. In order to limit the memory
// usage, here set max number of pending queued buffers to 2. If it causes some
// frame drop, just increase it to 3 or 4...
#define MAX_QUEUED_BUFFERS (2)

extern bool g_bProfiling;

VideoEncoder::VideoEncoder(const char *name, const char *outputFilename,
        int width, int height, uint32_t pixfmt) :
    m_name(name),
    m_width(width),
    m_height(height),
    m_pixfmt(pixfmt),
    m_outputFilename(outputFilename)
{
    m_VideoEncoder = NULL;
    m_outputFile = NULL;
}

VideoEncoder::~VideoEncoder()
{
    if (m_VideoEncoder)
        delete m_VideoEncoder;

    if (m_outputFile)
        delete m_outputFile;
}

bool VideoEncoder::initialize()
{
    // Create encoder
    if (!createVideoEncoder())
        ORIGINATE_ERROR("Could not create encoder.");

    // Create output file
    m_outputFile = new std::ofstream(m_outputFilename.c_str());
    if (!m_outputFile)
        ORIGINATE_ERROR("Failed to open output file.");

    // Stream on
    if (m_VideoEncoder->output_plane.setStreamStatus(true) < 0)
        ORIGINATE_ERROR("Failed to stream on output plane");
    if (m_VideoEncoder->capture_plane.setStreamStatus(true) < 0)
        ORIGINATE_ERROR("Failed to stream on capture plane");

    // Set DQ callback
    m_VideoEncoder->capture_plane.setDQThreadCallback(encoderCapturePlaneDqCallback);

    // startDQThread starts a thread internally which calls the dqThreadCallback
    // whenever a buffer is dequeued on the plane
    m_VideoEncoder->capture_plane.startDQThread(this);

    // Enqueue all the empty capture plane buffers
    for (uint32_t i = 0; i < m_VideoEncoder->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        m_VideoEncoder->capture_plane.qBuffer(v4l2_buf, NULL);
    }

    return true;
}

bool VideoEncoder::encodeFromFd(int dmabuf_fd)
{
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[MAX_PLANES];

    memset(&v4l2_buf, 0, sizeof(v4l2_buf));
    memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));
    v4l2_buf.m.planes = planes;

    if (m_VideoEncoder->output_plane.getNumQueuedBuffers() < MAX_QUEUED_BUFFERS)
    {
        v4l2_buf.index = m_VideoEncoder->output_plane.getNumQueuedBuffers();
        v4l2_buf.m.planes[0].m.fd = dmabuf_fd;
        v4l2_buf.m.planes[0].bytesused = 1; // byteused must be non-zero
        CHECK_ERROR(m_VideoEncoder->output_plane.qBuffer(v4l2_buf, NULL));
        m_dmabufFdSet.insert(dmabuf_fd);
    }
    else
    {
        CHECK_ERROR(m_VideoEncoder->output_plane.dqBuffer(v4l2_buf, NULL, NULL, 10));
        // Buffer done, execute callback
        m_callback(v4l2_buf.m.planes[0].m.fd, m_callbackArg);
        m_dmabufFdSet.erase(v4l2_buf.m.planes[0].m.fd);

        if (dmabuf_fd < 0)
        {
            // Send EOS
            v4l2_buf.m.planes[0].bytesused = 0;
        }
        else
        {
            v4l2_buf.m.planes[0].m.fd = dmabuf_fd;
            v4l2_buf.m.planes[0].bytesused = 1; // byteused must be non-zero
            m_dmabufFdSet.insert(dmabuf_fd);
        }
        CHECK_ERROR(m_VideoEncoder->output_plane.qBuffer(v4l2_buf, NULL));
    }

    return true;
}

bool VideoEncoder::shutdown()
{
    // Wait till capture plane DQ Thread finishes
    // i.e. all the capture plane buffers are dequeued
    m_VideoEncoder->capture_plane.waitForDQThread(2000);

    // Return all queued buffers in output plane
    assert(m_dmabufFdSet.size() == MAX_QUEUED_BUFFERS - 1); // EOS buffer
                                                            // is not in the set
    for (std::set<int>::iterator it = m_dmabufFdSet.begin();
            it != m_dmabufFdSet.end(); it++)
    {
        m_callback(*it, m_callbackArg);
    }
    m_dmabufFdSet.clear();

    // Print profiling result
    if (g_bProfiling)
        m_VideoEncoder->printProfilingStats(std::cout);

    if (m_VideoEncoder)
    {
        delete m_VideoEncoder;
        m_VideoEncoder = NULL;
    }

   if (m_outputFile)
    {
        delete m_outputFile;
        m_outputFile = NULL;
    }

    return false;
}

bool VideoEncoder::createVideoEncoder()
{
    int ret = 0;

    m_VideoEncoder = NvVideoEncoder::createVideoEncoder(m_name.c_str());
    if (!m_VideoEncoder)
        ORIGINATE_ERROR("Could not create m_VideoEncoderoder");

    // Enable profiing
    if (g_bProfiling)
        m_VideoEncoder->enableProfiling();

    ret = m_VideoEncoder->setCapturePlaneFormat(m_pixfmt, m_width,
                                    m_height, 2 * 1024 * 1024);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set capture plane format");

    ret = m_VideoEncoder->setOutputPlaneFormat(V4L2_PIX_FMT_YUV420M, m_width,
                                    m_height);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set output plane format");

    ret = m_VideoEncoder->setBitrate(4 * 1024 * 1024);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set bitrate");

    if (m_pixfmt == V4L2_PIX_FMT_H264)
    {
        ret = m_VideoEncoder->setProfile(V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);
    }
    else
    {
        ret = m_VideoEncoder->setProfile(V4L2_MPEG_VIDEO_H265_PROFILE_MAIN);
    }
    if (ret < 0)
        ORIGINATE_ERROR("Could not set m_VideoEncoderoder profile");

    if (m_pixfmt == V4L2_PIX_FMT_H264)
    {
        ret = m_VideoEncoder->setLevel(V4L2_MPEG_VIDEO_H264_LEVEL_5_0);
        if (ret < 0)
            ORIGINATE_ERROR("Could not set m_VideoEncoderoder level");
    }

    ret = m_VideoEncoder->setRateControlMode(V4L2_MPEG_VIDEO_BITRATE_MODE_CBR);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set rate control mode");

    ret = m_VideoEncoder->setIFrameInterval(30);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set I-frame interval");

    ret = m_VideoEncoder->setFrameRate(30, 1);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set m_VideoEncoderoder framerate");

    // Query, Export and Map the output plane buffers so that we can read
    // raw data into the buffers
    ret = m_VideoEncoder->output_plane.setupPlane(V4L2_MEMORY_DMABUF, 10, true, false);
    if (ret < 0)
        ORIGINATE_ERROR("Could not setup output plane");

    // Query, Export and Map the capture plane buffers so that we can write
    // encoded data from the buffers
    ret = m_VideoEncoder->capture_plane.setupPlane(V4L2_MEMORY_MMAP, 6, true, false);
    if (ret < 0)
        ORIGINATE_ERROR("Could not setup capture plane");

    return true;
}

bool
VideoEncoder::encoderCapturePlaneDqCallback(
        struct v4l2_buffer *v4l2_buf,
        NvBuffer *buffer,
        NvBuffer *shared_buffer)
{
    if (!v4l2_buf)
    {
        m_VideoEncoder->abort();
        ORIGINATE_ERROR("Failed to dequeue buffer from capture plane");
    }

    m_outputFile->write((char *) buffer->planes[0].data, buffer->planes[0].bytesused);

    m_VideoEncoder->capture_plane.qBuffer(*v4l2_buf, NULL);

    // GOT EOS from encoder. Stop dqthread.
    if (buffer->planes[0].bytesused == 0)
    {
        return false;
    }

    return true;
}

