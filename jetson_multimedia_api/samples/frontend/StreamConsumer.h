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

#ifndef __STREAMCONSUMER_H__
#define __STREAMCONSUMER_H__

#include <string>
#include <Argus/Argus.h>
#include <EGLStream/EGLStream.h>
#include <EGLStream/NV/ImageNativeBuffer.h>
#include "Thread.h"

using namespace Argus;
using namespace ArgusSamples;
using namespace EGLStream;

// Abstract class of Argus::OutputStream consumer
class StreamConsumer : public Thread
{
public:
    StreamConsumer(const char *name, Size2D<uint32_t> size);
    virtual ~StreamConsumer();

    // OutputStream used to receive frames from producer
    void setOutputStream(OutputStream *stream)
    {
        m_stream = stream;
    }

    // Stream resolution to initialize the OutputStream
    Size2D<uint32_t> getSize()
    {
        return m_size;
    }

    const char* getName()
    {
        return m_name.c_str();
    }

protected:
    std::string m_name;   // User defined name of this consumer
    OutputStream *m_stream;
    UniqueObj<FrameConsumer> m_consumer;
    Size2D<uint32_t> m_size;

    // Methods of Thread
    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();

    // Abstract methods
    virtual bool processFrame(Frame *frame) = 0;

    // Utility methods
    void Log(const char *fmt, ...);
};

#endif  // __STREAMCONSUMER_H__
