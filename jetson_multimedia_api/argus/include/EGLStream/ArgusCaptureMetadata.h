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

#ifndef _EGLSTREAM_ARGUS_CAPTURE_METADATA_H
#define _EGLSTREAM_ARGUS_CAPTURE_METADATA_H

namespace EGLStream
{

/**
 * @class IArgusCaptureMetadata
 *
 * This interface is used to access Argus::CaptureMetadata from an object.
 * Objects that may support this interface are EGLStream::Frame objects
 * originating from an Argus producer, or a MetadataContainer object
 * created directly from an EGLStream frame's embedded metadata.
 */
DEFINE_UUID(Argus::InterfaceID, IID_ARGUS_CAPTURE_METADATA, b94aa2e0,c3c8,11e5,a837,08,00,20,0c,9a,66);
class IArgusCaptureMetadata : public Argus::Interface
{
public:
    static const Argus::InterfaceID& id() { return IID_ARGUS_CAPTURE_METADATA; }

    /**
     * Returns the CaptureMetadata associated with the object.. The lifetime of this
     * metadata is equivalent to that of the object being called. NULL may be returned
     * if there is no metadata available.
     */
    virtual Argus::CaptureMetadata* getMetadata() const = 0;

protected:
    ~IArgusCaptureMetadata() {}
};

} // namespace EGLStream

#endif // _EGLSTREAM_ARGUS_CAPTURE_METADATA_H
