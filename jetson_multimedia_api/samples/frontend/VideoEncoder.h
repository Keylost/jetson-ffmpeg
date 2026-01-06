/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __VIDEOENCODER_H__
#define __VIDEOENCODER_H__

#include <fstream>
#include <iostream>
#include <set>
#include <NvVideoEncoder.h>

class NvBuffer;

/*
 * A helper class to simplify the usage of V4l2 encoder
 * Steps to use this class
 *   (1) Create the object
 *   (2) Call setBufferDoneCallback. The callback is called to return buffer to caller
 *   (3) Call initialize
 *   (4) Feed encoder by calling encodeFromFd
 *   (5) Call shutdown
 */
class VideoEncoder
{
public:
    VideoEncoder(const char *name, const char *outputFilename,
            int width, int height, uint32_t pixfmt = V4L2_PIX_FMT_H265);
    ~VideoEncoder();

    bool initialize();
    bool shutdown();

    // Encode API
    bool encodeFromFd(int dmabuf_fd);

    // Callbackt to return buffer
    void setBufferDoneCallback(void (*callback)(int, void*), void *arg)
    {
        m_callback = callback;
        m_callbackArg = arg;
    }

private:

    NvVideoEncoder *m_VideoEncoder;     // The V4L2 encoder
    bool createVideoEncoder();

    static bool encoderCapturePlaneDqCallback(
            struct v4l2_buffer *v4l2_buf,
            NvBuffer *buffer,
            NvBuffer *shared_buffer,
            void *arg)
    {
        VideoEncoder *thiz = static_cast<VideoEncoder*>(arg);
        return thiz->encoderCapturePlaneDqCallback(v4l2_buf, buffer, shared_buffer);
    }

    bool encoderCapturePlaneDqCallback(
            struct v4l2_buffer *v4l2_buf,
            NvBuffer *buffer,
            NvBuffer *shared_buffer);

    std::string m_name;     // name of the encoder
    int m_width;
    int m_height;
    uint32_t m_pixfmt;
    std::string m_outputFilename;
    std::ofstream *m_outputFile;
    std::set<int> m_dmabufFdSet;    // Collection to track all queued buffer
    void (*m_callback)(int, void*);        // Output plane DQ callback
    void *m_callbackArg;
};

#endif  // __VIDEOENCODER_H__
