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

#ifndef __VIDEOENCODESTREAMCONSUMER_H__
#define __VIDEOENCODESTREAMCONSUMER_H__

#include "StreamConsumer.h"
#include "VideoEncoder.h"
#include "Queue.h"

class VideoEncodeStreamConsumer : public StreamConsumer
{
public:
    VideoEncodeStreamConsumer(const char *name, const char *outputFilename,
            Size2D<uint32_t> size, uint32_t pixfmt = V4L2_PIX_FMT_H265);
    virtual ~VideoEncodeStreamConsumer();

    virtual bool threadInitialize();
    virtual bool threadShutdown();

    virtual bool processFrame(Frame *frame);

private:

    static void bufferDoneCallback(int dmabuf_fd, void *arg)
    {
        VideoEncodeStreamConsumer *thiz = static_cast<VideoEncodeStreamConsumer*>(arg);
        thiz->bufferDoneCallback(dmabuf_fd);
    }

    void bufferDoneCallback(int dmabuf_fd);

    VideoEncoder m_VideoEncoder;
    Queue<int> m_emptyBufferQueue;
};

#endif  // __VIDEOENCODESTREAMCONSUMER_H__
