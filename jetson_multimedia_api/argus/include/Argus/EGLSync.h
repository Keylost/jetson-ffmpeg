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
 * <b>Libargus API: EGLSync API</b>
 *
 * @b Description: Defines a SyncType that uses EGLSync objects.
 */

#ifndef _ARGUS_EGL_SYNC_H
#define _ARGUS_EGL_SYNC_H

namespace Argus
{

/**
 * @defgroup ArgusBufferEGLSync EGLSync
 * @ingroup ArgusBufferSync
 * Sync type that uses EGLSync objects (SYNC_TYPE_EGL_SYNC).
 */

/**
 * Sync type that uses EGLSync objects (SYNC_TYPE_EGL_SYNC).
 * @ingroup ArgusBufferSync
 */
DEFINE_UUID(SyncType, SYNC_TYPE_EGL_SYNC, 5df77c90,5d1b,11e7,9598,08,00,20,0c,9a,66);

/**
 * @class IEGLSync
 *
 * Interface that provides EGLSync input and output methods for a Buffer.
 *
 * @ingroup ArgusBufferEGLSync
 */
DEFINE_UUID(InterfaceID, IID_EGL_SYNC, 5df77c91,5d1b,11e7,9598,08,00,20,0c,9a,66);
class IEGLSync : public Interface
{
public:
    static const InterfaceID& id() { return IID_EGL_SYNC; }

    /**
     * Creates and returns a new EGLSync object that is signalled when all operations on the
     * Buffer from the previous libargus capture request have completed.
     *
     * When sync support is enabled for a Stream, libargus may output Buffers to that stream
     * even if hardware operations are still pending on the Buffer's image data. In this case,
     * libargus will attach sync information to the Buffer when it is acquired by the client
     * that must be used to block any client operations on the image data until all preceeding
     * libargus operations have completed. Failure to block on this sync information may lead
     * to undefined buffer contents.
     *
     * This method will create and output a new EGLSync object that will be signalled once all
     * libargus operations on the Buffer have completed. Ownership of this EGLSync object is
     * given to the caller, who must then wait on the sync object as needed before destroying
     * it using eglDestroySyncKHR. Calling this method more than once is allowed, and each call
     * will create and return a new EGLSync object.
     *
     * This method should only ever be called while the Buffer is in an acquired state; ie. the
     * time between when the Buffer was acquired by IBufferOutputStream::acquireBuffer and when
     * it was released by IBufferOutputStream::releaseBuffer. If called outside of the acquired
     * state, STATUS_UNAVAILABLE will be returned.
     *
     * When successful, STATUS_OK will be returned and 'eglSync' will be written with the new
     * EGLSync object. Note that EGL_NO_SYNC_KHR is still a valid output for the 'eglSync' even
     * when STATUS_OK is returned; this implies that libargus does not have any pending
     * operations to the Buffer and so the client need not take any sync precautions before
     * accessing the image data. Thus, the returned Status code should be used for detecting
     * failures rather than checking for an EGL_NO_SYNC_KHR output.
     *
     * @param[in] eglDisplay The EGLDisplay that shall own the returned EGLSync object.
     * @param[out] eglSync Output for the newly created EGLSync object. Ownership of this object
     *                     is given to the client.
     *
     * @returns success/status of this call.
     */
    virtual Status getAcquireSync(EGLDisplay eglDisplay, EGLSyncKHR* eglSync) = 0;

    /**
     * Sets the client-provided EGLSync for a Buffer prior to its release.
     *
     * When sync support is enabled for a Stream, the client may release Buffers back to
     * libargus for future capture use even if the client has hardware operations pending on
     * the Buffer's image data. In this case, the client must provide an EGLSync object to
     * libargus that will be signalled by the completion of the client's pending operations.
     * This sync object will then be waited on by libargus to prevent any buffer operations
     * from occuring before the client sync has been signalled.
     *
     * This method should only ever be called while the Buffer is in an acquired state; ie. the
     * time between when the Buffer was acquired by IBufferOutputStream::acquireBuffer and when
     * it was released by IBufferOutputStream::releaseBuffer. If called outside of this period,
     * STATUS_UNAVAILBLE will be returned and no object updates will be made. Otherwise, when
     * called in the acquired state, this method will set the EGLSync that will be provided to
     * libargus at the time that the Buffer is released by IBufferOutputStream::releaseBuffer
     * and STATUS_OK will be returned. On success, ownership of the EGLSync object will be passed
     * to libargus; further use of the EGLSync by the client after this point may lead to
     * undefined results or abnormal termination. If called more than once when in the acquired
     * state, any previously set EGLSync will be replaced; only the last set EGLSync before
     * calling releaseBuffer will be waited on by libargus.
     *
     * If the client does not have any pending operations on the Buffer at the time that
     * releaseBuffer is called, it is allowed for the client to either skip calling this method
     * or to call it using EGL_NO_DISPLAY and EGL_NO_SYNC_KHR such that libargus will not have
     * to consider any sync requirements and may use the Buffer immediately.
     *
     * @param[in] eglDisplay The EGLDisplay that created the EGLSync object being provided.
     * @param[in] eglSync The EGLSync that libargus must wait on before accessing the buffer.
     *
     * @returns success/status of this call.
     */
    virtual Status setReleaseSync(EGLDisplay eglDisplay, EGLSyncKHR eglSync) = 0;

protected:
    ~IEGLSync() {}
};

} // namespace Argus

#endif // _ARGUS_EGL_SYNC_H
