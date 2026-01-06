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

#ifndef _EGLSTREAM_NV_IMAGE_NATIVE_BUFFER_H
#define _EGLSTREAM_NV_IMAGE_NATIVE_BUFFER_H

#include <nvbuf_utils.h>

namespace EGLStream
{

/**
 * The NV::ImageNativeBuffer extension adds an interface to create and/or
 * copy EGLStream Images to NvBuffers (see nvbuf_utils.h).
 */
DEFINE_UUID(Argus::ExtensionName, NV_IMAGE_NATIVE_BUFFER, ce9e8c60,1792,11e6,bdf4,08,00,20,0c,9a,66);

namespace NV
{

/*
 * Counterclockwise rotation value, in degree
 */
enum Rotation
{
    ROTATION_0,
    ROTATION_90,
    ROTATION_180,
    ROTATION_270,
    ROTATION_COUNT
};

/**
 * @class IImageNativeBuffer
 *
 * Interface that supports creating new NvBuffers and/or copying Image contents
 * to existing NvBuffers.
 */
DEFINE_UUID(Argus::InterfaceID, IID_IMAGE_NATIVE_BUFFER, 2f410340,1793,11e6,bdf4,08,00,20,0c,9a,66);
class IImageNativeBuffer : public Argus::Interface
{
public:
    static const Argus::InterfaceID& id() { return IID_IMAGE_NATIVE_BUFFER; }

    /**
     * Creates a new NvBuffer, copies the image contents to the new buffer, then
     * returns the dmabuf-fd. Ownership of this dmabuf-fd is given to the caller
     * and must be destroyed using NvBufferDestroy (see nvbuf_utils.h).
     *
     * Note that the size, format, and layout of the new buffer can be different from
     * what is being used for the EGLStream, and if this is the case then scaling
     * and format conversion will be performed when the image is copied to the
     * new buffer. Details of this scaling and conversion are left up to the
     * implementation, but the application should consider and account for any
     * measured performance penalties associated with such operations.
     *
     * @param[in] size the size of the NvBuffer to create.
     * @param[in] format the color format to use for the new NvBuffer.
     * @param[in] layout the buffer layout to use for the new NvBuffer.
     * @param[in] rotation flag that could be 0/90/180/270 degree.
     * @param[out] status optional status return code.
     * @returns -1 on failure, or a valid dmabuf-fd on success.
     */
    virtual int createNvBuffer(Argus::Size2D<uint32_t> size,
                               NvBufferColorFormat format,
                               NvBufferLayout layout,
                               Rotation rotation = ROTATION_0,
                               Argus::Status* status = NULL) const = 0;

    /**
     * Copies the image contents to the given NvBuffer. This performs an uncropped
     * (full-surface) copy of the image to the provided buffer, which is permitted
     * to have different size, format, and layout attributes than those of the buffer
     * backing this EGLStream image. If this is the case, scaling and format conversion
     * will be performed when the image is copied to the buffer. Details of this scaling
     * and conversion are left up to the implementation, but the application should
     * consider and account for any measured performance penalties associated with such
     * operations.
     *
     * @param[in] fd the dmabuf-fd of the NvBuffer to copy to.
     * @param[in] rotation flag that could be 0/90/180/270 degree.
     */
    virtual Argus::Status copyToNvBuffer(int fd, Rotation rotation = ROTATION_0) const = 0;

protected:
    ~IImageNativeBuffer() {}
};

} // namespace NV

} // namespace EGLStream

#endif // _EGLSTREAM_NV_IMAGE_NATIVE_BUFFER_H
