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

#ifndef _EGLSTREAM_FRAME_H
#define _EGLSTREAM_FRAME_H

namespace EGLStream
{

class Image;

/**
 * Frame objects are acquired and returned by a FrameConsumer, and correspond
 * to frames that have been written to the stream. Frames contain metadata
 * corresponsing to the stream frame as well as the Image data of the frame.
 * Destroying a Frame will return its image buffers back to the stream for reuse.
 */
class Frame : public Argus::InterfaceProvider, public Argus::Destructable
{
protected:
    ~Frame() {}
};

/**
 * @class IFrame
 *
 * Interface that provides core access to a Frame.
 */
DEFINE_UUID(Argus::InterfaceID, IID_FRAME, 546F4520,87EF,11E5,A837,08,00,20,0C,9A,66);
class IFrame : public Argus::Interface
{
public:
    static const Argus::InterfaceID& id() { return IID_FRAME; }

    /**
     * Returns the frame number.
     */
    virtual uint64_t getNumber() const = 0;

    /**
     * Returns the timestamp of the frame, in nanoseconds.
     */
    virtual uint64_t getTime() const = 0;

    /**
     * Returns the Image contained in the Frame. The returned Image object is
     * owned by the Frame and is valid as long as the Frame is valid. (that is, while
     * the Frame is acquired).
     */
    virtual Image* getImage() = 0;

protected:
    ~IFrame() {}
};

} // namespace EGLStream

#endif // _EGLSTREAM_FRAME_H
