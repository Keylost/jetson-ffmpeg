/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
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
 * <b>Libargus API: BufferStream API</b>
 *
 * @b Description: Defines an OutputStream type used to write to application-managed buffers.
 */

#ifndef _ARGUS_BUFFER_STREAM_H
#define _ARGUS_BUFFER_STREAM_H

namespace Argus
{

/**
 * @defgroup ArgusBufferOutputStream BufferOutputStream
 * @ingroup ArgusOutputStream
 * @ref ArgusOutputStream type that writes to application-managed buffers (STREAM_TYPE_BUFFER).
 *
 * Buffer-based OutputStream objects maintain a set of Buffer objects that are
 * created by the application to wrap native image buffers allocated and owned
 * by the application. These Buffer objects do not take possession of the native
 * resources, which continue to be owned by the application; rather, they are used
 * to control data access between libargus (as capture results are written) and the
 * application (as the capture results are read).
 *
 * Every Buffer stream is associated with a single BufferType, which corresponds
 * to the native resource type that is being wrapped by its Buffers. This BufferType
 * dictates which interfaces will be supported by the OutputStream and the child
 * BufferSettings and Buffer objects created by it, and is immutable after stream
 * creation.
 *
 * In addition to image data, Buffer objects may be optionally used to transport
 * sync information between libargus and the application in order to support hardware
 * level synchronization and pipelining across the API boundary. The type of sync
 * information, and the sync interfaces supported by a Buffer, is controlled by
 * the SyncType.
 */
/**
 * @defgroup ArgusBufferOutputStreamSettings BufferOutputStreamSettings
 * @ingroup ArgusOutputStreamSettings
 * Settings type used to configure/create @ref ArgusBufferOutputStream streams (STREAM_TYPE_BUFFER).
 */

/**
 * @ref ArgusOutputStream type that writes to application-managed Buffers.
 * @ingroup ArgusOutputStreamSettings
 */
DEFINE_UUID(StreamType, STREAM_TYPE_BUFFER, c723d960,5231,11e7,9598,18,00,20,0c,9a,66);

/**
 * @defgroup ArgusBufferBuffer Buffer Types
 * @ingroup ArgusBuffer
 * The buffer type describes the type of the image resource being wrapped by the @ref ArgusBuffer.
 */
/**
 * @defgroup ArgusBufferBufferSettings Buffer Types
 * @ingroup ArgusBufferSettings
 * Provides buffer type specific configuration settings.
 */
DEFINE_NAMED_UUID_CLASS(BufferType);
DEFINE_UUID(BufferType, BUFFER_TYPE_NONE, c723d961,5231,11e7,9598,18,00,20,0c,9a,66);

/**
 * @defgroup ArgusBufferSync Sync Types
 * @ingroup ArgusBuffer
 * The sync type describes the type of sync object to use with the @ref ArgusBuffer.
 */
/**
 * @defgroup ArgusBufferBufferSettings Buffer Types
 * @ingroup ArgusBufferSettings
 * Provides sync type specific configuration settings.
 */
DEFINE_NAMED_UUID_CLASS(SyncType);
DEFINE_UUID(SyncType, SYNC_TYPE_NONE, c723d962,5231,11e7,9598,18,00,20,0c,9a,66);

/**
 * Object that wraps an application-managed buffer for use as a capture request destination.
 *
 * Every Buffer is associated with a single BufferType, which corresponds to the
 * native resource type that is being wrapped by it, and dictates which interfaces
 * it will support.
 *
 * In addition to image data, Buffer objects may optionally transport sync
 * information between libargus and the application in order to support hardware
 * level synchronization and pipelining across the API boundary. The type of sync
 * information, and the sync interfaces supported by a Buffer, is controlled by
 * the SyncType.
 *
 * All Buffer objects will support the IBuffer interface in order to query the
 * core BufferType and SyncType.
 *
 * @defgroup ArgusBuffer Buffer
 * @ingroup ArgusObjects
 */
class Buffer : public InterfaceProvider, public Destructable
{
protected:
    ~Buffer() {}
};

/**
 * Container for settings used to configure/create a @ref ArgusBuffer.
 *
 * These objects are created by IBufferOutputStream::createBufferSettings, and
 * are used to configure the various parameters required for Buffer creation.
 * Since the Buffer OutputStream which creates this object uses a single
 * BufferType and SyncType, the interfaces supported by BufferSettings objects
 * are dictated by these types.
 *
 * @defgroup ArgusBufferSettings BufferSettings
 * @ingroup ArgusObjects
 */
class BufferSettings : public InterfaceProvider, public Destructable
{
protected:
    ~BufferSettings() {}
};

/**
 * @class IBufferOutputStreamSettings
 *
 * Interface that exposes the configuration available to Buffer-based OutputStreams.
 *
 * @ingroup ArgusBufferOutputStreamSettings
 */
DEFINE_UUID(InterfaceID, IID_BUFFER_OUTPUT_STREAM_SETTINGS,
                         c723d963,5231,11e7,9598,18,00,20,0c,9a,66);
class IBufferOutputStreamSettings : public Interface
{
public:
    static const InterfaceID& id() { return IID_BUFFER_OUTPUT_STREAM_SETTINGS; }

    /**
     * Sets the BufferType for the stream. This controls which type of native buffer
     * type will be wrapped by this OutputStream, and thus will dictate which interfaces
     * are supported by the OutputStream and child BufferSettings and Buffer objects.
     * This value defaults to BUFFER_TYPE_NONE and must be set by the application
     * to a BufferType supported by this libargus implementation.
     *
     * @param[in] type The BufferType to use for the new OutputStream.
     */
    virtual Status setBufferType(const BufferType& type) = 0;

    /**
     * Returns the BufferType to be used for the stream.
     */
    virtual BufferType getBufferType() const = 0;

    /**
     * Sets the SyncType for the stream. This controls which type of native sync
     * information will be attached to Buffers for sync support between libargus
     * and the application.
     * This value defaults to SYNC_TYPE_NONE, which means that no sync information
     * will be supported. In this case, both the application and libargus are
     * expected to be done all read and/or write operations before passing the
     * Buffer to the other.
     *
     * @param[in] type The SyncType to use for the new OutputStream.
     */
    virtual Status setSyncType(const SyncType& type) = 0;

    /**
     * Returns the SyncType to be used for the stream.
     */
    virtual SyncType getSyncType() const = 0;

    /**
     * Sets the metadata enable for the stream. When metadata is enabled, a CaptureMetadata
     * object may be attached to each Buffer when it is output from libargus as the result
     * of a successful capture request (see IBuffer::getMetadata).
     *
     * @param[in] enable Whether or not metadata is enabled for the stream.
     */
    virtual void setMetadataEnable(bool enable) = 0;

    /**
     * Returns the metadata enable.
     */
    virtual bool getMetadataEnable() const = 0;

protected:
    ~IBufferOutputStreamSettings() {}
};

/**
 * @class IBufferOutputStream
 *
 * Interface that provides the methods used with Buffer-based OutputStreams.
 *
 * @ingroup ArgusBufferOutputStream
 */
DEFINE_UUID(InterfaceID, IID_BUFFER_OUTPUT_STREAM, c723d964,5231,11e7,9598,18,00,20,0c,9a,66);
class IBufferOutputStream : public Interface
{
public:
    static const InterfaceID& id() { return IID_BUFFER_OUTPUT_STREAM; }

    /**
     * Returns the BufferType of the stream.
     */
    virtual BufferType getBufferType() const = 0;

    /**
     * @returns the SyncType of the stream.
     */
    virtual SyncType getSyncType() const = 0;

    /**
     * Creates a BufferSettings object. This Destructable object is used to configure
     * the settings for a new Buffer object, including things like the native buffer
     * handle that is to be wrapped by the Buffer. The interfaces and settings that
     * are supported by the new BufferSettings object are dictated by the BufferType
     * and SyncType of the creating OutputStream.
     *
     * @param[out] status An optional pointer to return success/status.
     *
     * @returns a new BufferSettings, or NULL on failure (error code written to 'status').
     */
    virtual BufferSettings* createBufferSettings(Status* status = NULL) = 0;

    /**
     * Creates a Buffer object. All of the settings used to configure Buffer creation
     * are provided by the BufferSettings object (which continues to be owned by the
     * application and can be reused until destroyed).
     *
     * New Buffer objects are returned to the application in the "acquired" state,
     * meaning that the application must call releaseBuffer on the Buffer before it
     * may be used by libargus.
     *
     * @param[in] settings the buffer settings to use for Buffer creation.
     * @param[out] status An optional pointer to return success/status.
     *
     * @returns a new BufferSettings, or NULL on failure (error code written to 'status').
     */
    virtual Buffer* createBuffer(const BufferSettings* settings, Status* status = NULL) = 0;

    /**
     * Acquires a Buffer from the stream that was written to by a libargus capture request.
     *
     * Buffers are acquired from the stream in FIFO order relative to when they are
     * produced by libargus (which may not match the original request submission order).
     * If a non-zero timeout is provided, this operation will block until a new Buffer
     * is produced by libargus or the timeout period is exceeded.
     *
     * Once a Buffer has been acquired, the application will have exclusive access to the
     * Buffer's image data, which it will retain until the Buffer is released back to the
     * stream for further capture request use via releaseBuffer. Buffers may also be
     * destroyed while acquired; doing so prevents any further use of the Buffer object
     * within the Stream and releases any buffer resources or references held by the Buffer
     * object.
     *
     * If sync support has been enabled for this Stream/Buffer (ie. SyncType is not
     * STREAM_TYPE_NONE), hardware synchronization capabilities may be used to allow hardware
     * operations on a Buffer to still be pending when it is acquired from or released back to
     * libargus. In this case, the returned Buffer will contain the output sync information
     * provided by libargus which the application must obey before accessing the Buffer's
     * image data. Similarly, the application may need to write input sync information to
     * the Buffer before calling releaseBuffer such that libargus will obey the sync before
     * the Buffer is written to by a new capture request. The exact mechanism used for reading
     * and writing this sync state depends on and is documented by the various SyncTypes and
     * their corresponding interfaces.
     *
     * @param[in] timeout The amount of time to allow for the acquire.
     * @param[out] status An optional pointer to return success/status.
     *
     * @returns A Buffer that has been written to by a capture request
     */
    virtual Buffer* acquireBuffer(uint64_t timeout = TIMEOUT_INFINITE, Status* status = NULL) = 0;

    /**
     * Release a Buffer back to the stream to make it available for a future capture request.
     *
     * Once a Buffer has been released to the Stream, libargus will have exclusive access to
     * the Buffer's image resources until it is once again acquired by the application via
     * acquireBuffer. Any buffer access outside of libargus during this time may lead to
     * undefined results.
     *
     * While it is often the case that Buffers may be used by libargus in the order they are
     * released, this is not a requirement; libargus may reuse Buffers in any order once they
     * have been released.
     *
     * If sync support has been enabled for this StreamBuffer (ie. SyncType is not
     * STREAM_TYPE_NONE), sync information may need to be written to the Buffer by the client
     * before releaseBuffer is called. The exact mechanism used for writing this sync state
     * depends on and is documented by the various SyncTypes and their corresponding interfaces.
     *
     * Note that while it is safe to destroy a Buffer object while it has been released to
     * libargus, it is possible that pending requests may be using this Buffer and may still
     * output Events that reference the Buffer object, and so the application is responsible
     * for making sure that it does not use any Buffer object that it has previously destroyed.
     * If there are no pending requests using a particular Stream, destroying any of its
     * released Buffers will prevent them from ever being used or returned by libargus again.
     *
     * @param[in] buffer The Buffer to release back to the stream.
     */
    virtual Status releaseBuffer(Buffer* buffer) = 0;

    /**
     * Signals the end of the stream.
     *
     * Once the end of stream has been signalled on a stream, any call made to acquireBuffer
     * will immediately (ignoring the timeout parameter) return NULL with a STATUS_END_OF_STREAM
     * status when the following is true:
     *   1) There are no Buffers immediately available to be acquired, and
     *   2) There are no capture requests pending writes to the stream.
     * This implies that no pending or completed frames will be lost, and that all pending or
     * completed frames must be acquired before an END_OF_STREAM status is returned.
     *
     * If any thread is blocked in acquireBuffer when the end of stream is signalled, and the
     * above conditions are met, then that thread will unblock and return END_OF_STREAM immediately.
     */
    virtual Status endOfStream() = 0;

protected:
    ~IBufferOutputStream() {}
};

/**
 * @class IBuffer
 *
 * Interface that provides the core methods for Buffer objects.
 *
 * @ingroup ArgusBuffer
 */
DEFINE_UUID(InterfaceID, IID_BUFFER, c723d965,5231,11e7,9598,18,00,20,0c,9a,66);
class IBuffer : public Interface
{
public:
    static const InterfaceID& id() { return IID_BUFFER; }

    /**
     * Returns the BufferType of the Buffer.
     */
    virtual BufferType getBufferType() const = 0;

    /**
     * Returns the SyncType of the Buffer.
     */
    virtual SyncType getSyncType() const = 0;

    /**
     * Sets the client data for the Buffer.
     * This is provided as a convenience for applications to be able to map Buffers to
     * other client-managed data. It is not used at all by the libargus implementation,
     * and is returned as-is by getClientData.
     *   Default value: NULL
     *
     * @param[in] clientData The client data pointer to set in the buffer.
     */
    virtual void setClientData(const void* clientData) = 0;

    /**
     * Returns the client data from the Buffer.
     */
    virtual const void* getClientData() const = 0;

    /**
     * Returns the CaptureMetadata object that was attached to this Buffer when it was last
     * output to the stream from the result of a successful capture request.
     *
     * This method should only ever be called while the Buffer is in an acquired state; ie. the
     * time between when the Buffer was acquired by IBufferOutputStream::acquireBuffer and when
     * it was released by IBufferOutputStream::releaseBuffer. If called outside of the acquired
     * state, NULL will be returned. Similarly, the returned object will only remain valid so
     * long as the Buffer is acquired -- if this object or any of its interfaces are accessed
     * outside of the acquired state, undefined results or abnormal process termination may occur.
     *
     * Metadata will only be written if metadata is enabled for the stream (see
     * IBufferOutputStreamSettings::setMetadataEnable). NULL may also still be returned if there
     * were any capture errors or metadata is otherwise unavailable.
     */
    virtual const CaptureMetadata* getMetadata() const = 0;

protected:
    ~IBuffer() {}
};

} // namespace Argus

#endif // _ARGUS_BUFFER_STREAM_H
