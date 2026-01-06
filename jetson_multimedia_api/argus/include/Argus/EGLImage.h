/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
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
 * <b>Libargus API: EGLImage API</b>
 *
 * @b Description: Defines a BufferType that wraps an EGLImage resource.
 */

#ifndef _ARGUS_EGL_IMAGE_H
#define _ARGUS_EGL_IMAGE_H

namespace Argus
{

/**
 * @defgroup ArgusEGLImageBuffer EGLImageBuffer
 * @ingroup ArgusBufferBuffer
 * @ref ArgusBuffer type that wraps an EGLImage resource (BUFFER_TYPE_EGL_IMAGE).
 */
/**
 * @defgroup ArgusEGLImageBufferSettings EGLImageBufferSettings
 * @ingroup ArgusBufferBufferSettings
 * Settings type used to configure/create @ref ArgusEGLImageBuffer Buffers (BUFFER_TYPE_EGL_IMAGE).
 */

/**
 * @ref ArgusBuffer type that wraps an EGLImage resource.
 * @ingroup ArgusBufferBufferSettings
 */
DEFINE_UUID(BufferType, BUFFER_TYPE_EGL_IMAGE, c723d966,5231,11e7,9598,18,00,20,0c,9a,66);

/**
 * @class IEGLImageBufferSettings
 *
 * Interface that provides the settings used to configure EGLImage Buffer creation.
 * These Buffers act as siblings for the EGLImage, providing libargus write access
 * to the underlying buffer resources for the destination of capture requests.
 *
 * @ingroup ArgusEGLImageBufferSettings
 */
DEFINE_UUID(InterfaceID, IID_EGL_IMAGE_BUFFER_SETTINGS, c723d967,5231,11e7,9598,18,00,20,0c,9a,66);
class IEGLImageBufferSettings : public Interface
{
public:
    static const InterfaceID& id() { return IID_EGL_IMAGE_BUFFER_SETTINGS; }

    /**
     * Sets the EGLDisplay which owns the EGLImage.
     * @param[in] eglDisplay The EGLDisplay that owns the EGLImage.
     */
    virtual Status setEGLDisplay(EGLDisplay eglDisplay) = 0;

    /**
     * Returns the EGLDisplay which owns the EGLImage.
     */
    virtual EGLDisplay getEGLDisplay() const = 0;

    /**
     * Sets the EGLImage to use as the sibling for this Buffer.
     * @param[in] eglImage The EGLImage to use as the sibling for this Buffer.
     */
    virtual Status setEGLImage(EGLImageKHR eglImage) = 0;

    /**
     * Returns the EGLImage to use as the sibling for this Buffer.
     */
    virtual EGLImageKHR getEGLImage() const = 0;

protected:
    ~IEGLImageBufferSettings() {}
};

/**
 * @class IEGLImageBuffer
 *
 * Interface that provides methods to EGLImage Buffers.
 *
 * @ingroup ArgusEGLImageBuffer
 */
DEFINE_UUID(InterfaceID, IID_EGL_IMAGE_BUFFER, c723d968,5231,11e7,9598,18,00,20,0c,9a,66);
class IEGLImageBuffer : public Interface
{
public:
    static const InterfaceID& id() { return IID_EGL_IMAGE_BUFFER; }

    /**
     * Returns the EGLDisplay that owns the EGLImage.
     */
    virtual EGLDisplay getEGLDisplay() const = 0;

    /**
     * Returns the EGLImage being used for this Buffer.
     */
    virtual EGLImageKHR getEGLImage() const = 0;

protected:
    ~IEGLImageBuffer() {}
};

} // namespace Argus

#endif // _ARGUS_EGL_IMAGE_H
