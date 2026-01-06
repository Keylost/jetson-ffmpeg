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

/**
 * @file
 * <b>Libargus API: Stream API</b>
 *
 * @b Description: Defines stream related objects and interfaces.
 */

#ifndef _ARGUS_STREAM_H
#define _ARGUS_STREAM_H

namespace Argus
{

/**
 * The general operation, buffer source, and interfaces supported by a stream
 * object are defined by its core StreamType. The only StreamType currently
 * supported is STREAM_TYPE_EGL (see EGLStream.h).
 */
DEFINE_NAMED_UUID_CLASS(StreamType);

/**
 * Object representing an output stream capable of receiving image frames from a capture.
 *
 * OutputStream objects are used as the destination for image frames output from
 * capture requests. The operation of a stream, the source for its buffers, and the
 * interfaces it supports depend on the StreamType of the stream.
 *
 * @defgroup ArgusOutputStream OutputStream
 * @ingroup ArgusObjects
 */
class OutputStream : public InterfaceProvider, public Destructable
{
protected:
    ~OutputStream() {}
};

/**
 * Container for settings used to configure/create an OutputStream.
 *
 * The interfaces and configuration supported by these settings objects
 * depend on the StreamType that was provided during settings creation
 * (see ICaptureSession::createOutputStreamSettings).
 * These objects are passed to ICaptureSession::createOutputStream to create
 * OutputStream objects, after which they may be destroyed.
 *
 * @defgroup ArgusOutputStreamSettings OutputStreamSettings
 * @ingroup ArgusObjects
 */
class OutputStreamSettings : public InterfaceProvider, public Destructable
{
protected:
    ~OutputStreamSettings() {}
};

/**
 * @class IOutputStreamSettings
 *
 * Interface that exposes the settings common to all OutputStream types.
 *
 * @ingroup ArgusOutputStreamSettings
 */
DEFINE_UUID(InterfaceID, IID_OUTPUT_STREAM_SETTINGS, 52f2b830,3d52,11e6,bdf4,08,00,20,0c,9a,66);
class IOutputStreamSettings : public Interface
{
public:
    static const InterfaceID& id() { return IID_OUTPUT_STREAM_SETTINGS; }

    /**
     * Set the camera device to use as the source for this stream.
     *   Default value: First available device in the session.
     */
    virtual Status setCameraDevice(CameraDevice* device) = 0;
    virtual CameraDevice* getCameraDevice() const = 0;

protected:
    ~IOutputStreamSettings() {}
};

} // namespace Argus

#endif // _ARGUS_STREAM_H
