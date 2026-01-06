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
 * <b>Libargus API: Capture Session API</b>
 *
 * @b Description: Defines the CaptureSession object and interface.
 */

#ifndef _ARGUS_CAPTURE_SESSION_H
#define _ARGUS_CAPTURE_SESSION_H

namespace Argus
{

/**
 * Object that controls all operations on a single sensor.
 *
 * A capture session is bound to a single sensor (or, in future, a group of synchronized sensors)
 * and provides methods to perform captures on that sensor (via the ICaptureSession interface).
 *
 * @defgroup ArgusCaptureSession CaptureSession
 * @ingroup ArgusObjects
 */
class CaptureSession : public InterfaceProvider, public Destructable
{
protected:
    ~CaptureSession() {}
};

/**
 * @class ICaptureSession
 *
 * Interface to the core CaptureSession methods.
 *
 * @ingroup ArgusCaptureSession
 */
DEFINE_UUID(InterfaceID, IID_CAPTURE_SESSION, 813644f5,bc21,4013,af44,dd,da,b5,7a,9d,13);
class ICaptureSession : public Interface
{
public:
    static const InterfaceID& id() { return IID_CAPTURE_SESSION; }

    /**
     * Removes all previously submitted requests from the queue. When all requests
     * are cancelled, both the FIFO and the streaming requests will be removed.
     * If repeat captures are enabled, an implicit call to ICaptureSession::stopRepeat()
     * will be made before cancelling the requests.
     *
     * @returns success/status of this call.
     */
    virtual Status cancelRequests() = 0;

    /**
     * Submits a single capture request.
     * For blocking capture session (created by ICameraProvider::createBlockingCaptureSession),
     * it will wait until the request is accepted by lower level driver.
     * For non-blocking capture session (created by ICameraProvider::createCaptureSession),
     * it will queue a copy of the request to a queue and return.
     *
     * The client can submit the same request instance in a future call.
     * The request will be copied by the runtime.
     *
     * @param[in] request Parameters for the capture.
     * @param[in] timeout The timeout in nanoseconds. The camera device will
     * try to issue the request within the timeout period. If it can't it
     * will return and set @c status to STATUS_UNAVAILABLE.
     * @param[out] status An optional pointer to return success/status.
     *
     * @returns the capture id, a number that uniquely identifies (within this session) the request.
     * If the submission request failed, zero will be returned.
     * The request could fail because the timeout is reached,
     * or because some parameter(s) of the @c request are invalid.
     */
    virtual uint32_t capture(const Request* request,
                             uint64_t timeout = TIMEOUT_INFINITE,
                             Status* status = NULL) = 0;

    /**
     * Submits a burst of requests.
     *
     * For blocking capture session (created by ICameraProvider::createBlockingCaptureSession),
     * it will wait until the first request is accepted by lower level driver.
     * For non-blocking capture session (created by ICameraProvider::createCaptureSession),
     * it will queue a copy of the requests to a queue and return.

     * The runtime will either accept the entire burst or refuse it completely
     * (that is, no partial bursts will be accepted).
     *
     * @param[in] requestList The list of requests that make up the burst.
     * @param[in] timeout The timeout in nanoseconds. The camera device will try to issue
     * the request within the timeout period. If it can't it will return and set
     * @c status to STATUS_UNAVAILABLE.
     * @param[out] status An optional pointer to return success/status.
     *
     * @returns the capture id of the capture associated with the first request in the burst.
     * The capture id will increment by one for the captures associated with each successive
     * request.
     * If the submission request failed, zero will be returned.
     * The request could fail because the timeout is reached,
     * or because some parameter(s) of the @c request are invalid.
     */
    virtual uint32_t captureBurst(const std::vector<const Request*>& requestList,
                                  uint64_t timeout = TIMEOUT_INFINITE,
                                  Status* status = NULL) = 0;

    /**
     * Returns the maximum number of capture requests that can be included in a burst capture.
     */
    virtual uint32_t maxBurstRequests() const = 0;

    /**
     * Creates a request object that can be later used with this CaptureSession.
     *
     * @param[in] intent Optional parameter that specifies the intent of the capture request and
     * instructs the driver to populate the request with recommended settings
     * for that intent.
     * @param[out] status An optional pointer to return success/status.
     *
     * @see ICaptureMetadata::getClientData()
     */
    virtual Request* createRequest(const CaptureIntent& intent = CAPTURE_INTENT_PREVIEW,
                                   Status* status = NULL) = 0;

    /**
     * Creates an OutputStreamSettings object that is used to configure the creation of
     * an OutputStream (see createOutputStream). The type of OutputStream that will be
     * configured and created by these settings are determined by the StreamType.
     *
     * @param[in] type The type of the OutputStream to configure/create with these settings.
     * @param[out] status An optional pointer to return success/status.
     *
     * @returns The newly created OutputStreamSettings, or NULL on failure.
     */
    virtual OutputStreamSettings* createOutputStreamSettings(const StreamType& type,
                                                             Status* status = NULL) = 0;

    /**
     * Creates an OutputStream object using the settings configured by an OutputStreamSettings
     * object (see createOutputStreamSettings).
     *
     * @param[in] settings The settings to use for the new output stream.
     * @param[out] status An optional pointer to return success/status.
     *
     * @returns The newly created OutputStream, or NULL on failure.
     */
    virtual OutputStream* createOutputStream(const OutputStreamSettings* settings,
                                             Status* status = NULL) = 0;

    /**
     * Returns true if there is a streaming request in place.
     */
    virtual bool isRepeating() const = 0;

    /**
     * Sets up a repeating request. This is a convenience method that will queue
     * a request whenever the request queue is empty and the camera is ready to
     * accept new requests.
     *
     * To stop repeating the request, call stopRepeat().
     *
     * @param[in] request The request to repeat.
     *
     * @returns success/status of the call.
     */
    virtual Status repeat(const Request* request) = 0;

    /**
     * Sets up a repeating burst request. This is a convenience method that will queue
     * a request whenever the request queue is empty and the camera is ready to
     * accept new requests.
     *
     * To stop repeating the requests, call stopRepeat().
     *
     * @param[in] requestList The list of requests that make up the repeating burst.
     *
     * @returns success/status of the call.
     */
    virtual Status repeatBurst(const std::vector<const Request*>& requestList) = 0;

    /**
     * Shuts down any repeating capture.
     *
     * @returns The range of capture ids generated by the most recent repeat() / repeatBurst() call.
     * Note that some captures within that range may have been generated by explicit capture() calls
     * made while the repeating capture was in force.
     * If no captures were generated by the most recent repeat() / repeatBurst() call,
     * <tt>Range<uint32_t>(0,0)</tt> will be returned.
     */
    virtual Range<uint32_t> stopRepeat() = 0;

    /**
     * Waits until all pending captures are complete.
     *
     * @param[in] timeout The timeout value (in nanoseconds) for this call.
     * If the pipe has not become idle when the timeout expires,
     * the call will return STATUS_TIMEOUT.
     */
    virtual Status waitForIdle(uint64_t timeout = TIMEOUT_INFINITE) const = 0;

protected:
    ~ICaptureSession() {}
};

} // namespace Argus

#endif // _ARGUS_CAPTURE_SESSION_H
