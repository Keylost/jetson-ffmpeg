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

#ifndef _EGLSTREAM_METADATA_CONTAINER_H
#define _EGLSTREAM_METADATA_CONTAINER_H

namespace EGLStream
{

/**
 * @class MetadataContainer
 *
 * When image frames are presented to EGLStreams, private metadata may be
 * embedded in the frame data. This class provides a means for consumer-side
 * applications to extract and access this metadata directly from the EGLStream
 * without needing to initialize the producer library.
 */
class MetadataContainer : public Argus::InterfaceProvider, public Argus::Destructable
{
public:
    enum MetadataFrame
    {
        CONSUMER,
        PRODUCER
    };

    /**
     * Create and return a MetadataContainer object from the metadata embedded
     * in the EGLStream frame.
     * @param[in] eglDisplay The EGL display that owns the stream.
     * @param[in] eglStream The EGL stream.
     * @param[in] frame The frame for which the metadata should be extracted.
     *                  This can be either CONSUMER or PRODUCER, corresponding to the last
     *                  frame acquired by the consumer or presented by the producer, respectively.
     * @param[out] status Optional pointer to return success/status of the call.
     */
    static MetadataContainer* create(EGLDisplay eglDisplay,
                                     EGLStreamKHR eglStream,
                                     MetadataFrame frame = CONSUMER,
                                     Argus::Status* status = NULL);
protected:
    ~MetadataContainer() {}
};

} // namespace EGLStream

#endif // _EGLSTREAM_METADATA_CONTAINER_H
