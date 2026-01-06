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
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>
#include "StreamConsumer.h"
#include "Error.h"

// Globals.

StreamConsumer::StreamConsumer(const char *name, Size2D<uint32_t> size) :
    m_name(name),
    m_size(size)
{
}

StreamConsumer::~StreamConsumer()
{
}

bool StreamConsumer::threadInitialize()
{
    // Create the FrameConsumer.
    m_consumer = UniqueObj<FrameConsumer>(FrameConsumer::create(m_stream));
    if (!m_consumer)
        ORIGINATE_ERROR("Failed to create FrameConsumer");

    return true;
}

bool StreamConsumer::threadExecute()
{
    IEGLOutputStream *iEglOutputStream = interface_cast<IEGLOutputStream>(m_stream);
    IFrameConsumer *iFrameConsumer = interface_cast<IFrameConsumer>(m_consumer);

    // Wait until the producer has connected to the stream.
    Log("Waiting until producer is connected...\n");
    if (iEglOutputStream->waitUntilConnected() != STATUS_OK)
        ORIGINATE_ERROR("Stream failed to connect.");
    Log("Producer has connected; continuing.\n");

    while (true)
    {
        UniqueObj<Frame> frame(iFrameConsumer->acquireFrame());
        if (!processFrame(frame.get()))
            break;
    }

    requestShutdown();

    return true;
}

bool StreamConsumer::threadShutdown()
{
    return true;
}

void StreamConsumer::Log(const char *fmt, ...)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    printf("[%ld.%06ld] %s: ", ts.tv_sec, ts.tv_nsec / 1000,  m_name.c_str());

    va_list va;
    va_start(va, fmt);
    vprintf(fmt, va);
    va_end(va);
}

