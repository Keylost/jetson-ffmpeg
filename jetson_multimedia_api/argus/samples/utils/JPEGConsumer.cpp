/*
 * Copyright (c) 2016 - 2017, NVIDIA CORPORATION. All rights reserved.
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

#include "JPEGConsumer.h"
#include "Error.h"

#include <Argus/Argus.h>
#include <string.h>
#include <stdlib.h>
#include <sstream>
#include <iomanip>


namespace ArgusSamples
{

#define JPEG_CONSUMER_PRINT(...)    printf("JPEG CONSUMER: " __VA_ARGS__)

#ifdef ANDROID
#define JPEG_PREFIX "/sdcard/DCIM/Argus_"
#else
#define JPEG_PREFIX "Argus_"
#endif

bool JPEGConsumerThread::threadInitialize()
{
    // Create the FrameConsumer.
    m_consumer = UniqueObj<FrameConsumer>(FrameConsumer::create(m_stream));
    if (!m_consumer)
        ORIGINATE_ERROR("Failed to create FrameConsumer");

    return true;
}

bool JPEGConsumerThread::threadExecute()
{
    IEGLOutputStream *iEGLOutputStream = interface_cast<IEGLOutputStream>(m_stream);
    IFrameConsumer *iFrameConsumer = interface_cast<IFrameConsumer>(m_consumer);

    // Wait until the producer has connected to the stream.
    JPEG_CONSUMER_PRINT("Waiting until producer is connected...\n");
    if (iEGLOutputStream->waitUntilConnected() != STATUS_OK)
        ORIGINATE_ERROR("Stream failed to connect.");
    JPEG_CONSUMER_PRINT("Producer has connected; continuing.\n");;

    int frameCount = 0;
    while (true)
    {
        // Acquire a Frame.
        UniqueObj<Frame> frame(iFrameConsumer->acquireFrame());
        IFrame *iFrame = interface_cast<IFrame>(frame);
        if (!iFrame)
            break;

        // Get the Frame's Image.
        Image *image = iFrame->getImage();
        IImageJPEG *iJPEG = interface_cast<IImageJPEG>(image);
        if (!iJPEG)
            ORIGINATE_ERROR("Failed to get IImageJPEG interface.");

        // Write the Image to disk as JPEG.
        std::ostringstream fileName;
        fileName << JPEG_PREFIX << std::setfill('0') << std::setw(4) << frameCount++ << ".jpg";
        if (iJPEG->writeJPEG(fileName.str().c_str()) == STATUS_OK)
            JPEG_CONSUMER_PRINT("Captured a still image to '%s'\n", fileName.str().c_str());
        else
            ORIGINATE_ERROR("Failed to write JPEG to '%s'\n", fileName.str().c_str());
    }

    JPEG_CONSUMER_PRINT("No more frames. Cleaning up.\n");

    PROPAGATE_ERROR(requestShutdown());

    return true;
}

bool JPEGConsumerThread::threadShutdown()
{
    JPEG_CONSUMER_PRINT("Done.\n");

    return true;
}

} // namespace ArgusSamples
