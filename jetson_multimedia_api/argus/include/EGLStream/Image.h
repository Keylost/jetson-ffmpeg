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

#ifndef _EGLSTREAM_IMAGE_H
#define _EGLSTREAM_IMAGE_H

namespace EGLStream
{

/**
 * Image objects wrap the image data included in an acquired Frame.
 */

class Image : public Argus::InterfaceProvider
{
protected:
    ~Image() {}
};

/**
 * @class IImage
 *
 * Interface to provide the core functions for an Image.
 */
DEFINE_UUID(Argus::InterfaceID, IID_IMAGE, 546F4522,87EF,11E5,A837,08,00,20,0C,9A,66);
class IImage : public Argus::Interface
{
public:
    static const Argus::InterfaceID& id() { return IID_IMAGE; }

    /**
     * Returns the number of buffers in the Image.
     */
    virtual uint32_t getBufferCount() const = 0;

    /**
     * Returns the size of one of the Image's buffers.
     * @param[in] index The index of the buffer whose size to return (defaults to 0).
     */
    virtual uint64_t getBufferSize(uint32_t index = 0) const = 0;

    /**
     * Maps a buffer for CPU access and returns the mapped pointer.
     * How this data is laid out in memory may be described by another Frame interface,
     * or if the pixel format is UNKNOWN then it should be defined by the stream's producer.
     * @param[in] index The buffer index to map.
     * @param[out] status An optional pointer to return an error status code.
     */
    virtual const void* mapBuffer(uint32_t index, Argus::Status* status = NULL) = 0;

    /**
     * Maps the first/only buffer for CPU access and returns the mapped pointer.
     * How this data is laid out in memory may be described by another Frame interface,
     * or if the pixel format is UNKNOWN then it should be defined by the stream's producer.
     * @param[out] status An optional pointer to return an error status code.
     */
    virtual const void* mapBuffer(Argus::Status* status = NULL) = 0;

protected:
    ~IImage() {}
};

/**
 * @class IImage2D
 *
 * Interface that describes a 2D Image.
 *
 * Note that some 2D Image formats are composed of multiple 2D planes -- for
 * example, the color planes of a YUV image, or the buffer pyramid of a mipmap
 * stack. Each buffer in the image corresponds to an image plane, and the index
 * parameters used by this interfaces are identical to those used in IImage.
 */
DEFINE_UUID(Argus::InterfaceID, IID_IMAGE_2D, 546F4525,87EF,11E5,A837,08,00,20,0C,9A,66);
class IImage2D : public Argus::Interface
{
public:
    static const Argus::InterfaceID& id() { return IID_IMAGE_2D; }

    /**
     * Returns the size of the image plane, in pixels.
     * @param[in] index buffer index to get the size of (defaults to 0).
     */
    virtual Argus::Size2D<uint32_t> getSize(uint32_t index = 0) const = 0;

    /**
     * Returns the stride, or bytes per pixel row, of the image plane.
     * @param[in] index buffer index to get the width of (defaults to 0).
     */
    virtual uint32_t getStride(uint32_t index = 0) const = 0;

protected:
    ~IImage2D() {}
};

/**
 * @class IImageJPEG
 *
 * Provides a method to encode Images as JPEG data and write to disk.
 */
DEFINE_UUID(Argus::InterfaceID, IID_IMAGE_JPEG, 48aeddc9,c8d8,11e5,a837,08,00,20,0c,9a,66);
class IImageJPEG : public Argus::Interface
{
public:
    static const Argus::InterfaceID& id() { return IID_IMAGE_JPEG; }

    /**
     * Encodes the Image to JPEG and write to disk. This call blocks
     * until writing of the file is complete.
     * @param[in] path The file path to write the JPEG to.
     */
    virtual Argus::Status writeJPEG(const char* path) const = 0;

protected:
    ~IImageJPEG() {}
};

/**
 * @class IImageHeaderlessFile
 *
 * Provides a method to write image data to disk with no encoding,
 * and no header.
 *
 * All pixels are written to file in buffer, row, column order, with
 * multi-byte pixels stored little-endian.
 *
 * Filename should specify width and height, and pixel/layout i.e. NV12, P016,
 * or a specific Bayer layout.
 */
DEFINE_UUID(Argus::InterfaceID, IID_IMAGE_HEADERLESS_FILE,
            03018970,9254,11e7,9598,08,00,20,0c,9a,66);
class IImageHeaderlessFile : public Argus::Interface
{
public:
    static const Argus::InterfaceID& id() { return IID_IMAGE_HEADERLESS_FILE; }

    /**
     * Writes the pixels to disk.
     * This call blocks until writing of the file is complete.
     * @param[in] path The file path to write.
     */
    virtual Argus::Status writeHeaderlessFile(const char* path) const = 0;

protected:
    ~IImageHeaderlessFile() {}
};

} // namespace EGLStream

#endif // _EGLSTREAM_IMAGE_H
