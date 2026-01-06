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
 * <b>Libargus Extension: Internal Frame Count</b>
 *
 * @b Description: This file defines the InternalFrameCount extension.
 * @ingroup ArgusExtInternalFrameCount
 */

#ifndef _ARGUS_INTERNAL_FRAME_COUNT_H
#define _ARGUS_INTERNAL_FRAME_COUNT_H

namespace Argus
{

/**
 * Adds accessors for an internal frame count performance metric.
 * The "internal frame count" is an implementation-dependent metric that may be
 * used to detect performance issues and producer frame drops for libargus
 * implementations that make use of internal captures.
 *
 * When a device is opened by a CaptureSession, the libargus implementation may
 * begin to immediately capture and process frames from the device in order to
 * initialize the camera subsystem even before a client request has been
 * submitted. Similarly, frames may be captured and processed by the
 * implementation when the client is idle or not ready for output in order to
 * maintain the driver subsystem and/or auto-control state (exposure, white
 * balance, etc). These captures are started and processed entirely within the
 * libargus implementation, with no inputs from or outputs to the client
 * application, and so are referred to as "internal" captures. These internal
 * captures are typically submitted when there are no client requests in the
 * capture queue or no stream buffers available for output within a sensor
 * frame period, and so knowing when an internal capture has been submitted can
 * be used to detect application or performance issues in cases where these
 * conditions are not expected to occur. This extension provides this
 * information in the form of an "internal frame count", which is the total
 * number of captures submitted by the session including both the internal
 * captures as well as client-submitted requests. If an internal frame count
 * gap appears between two client-submitted captures, this means that one or
 * more internal captures have been performed.
 *
 * When an application is saturating the capture queue to maintain driver
 * efficiency, either manually or by using repeat capture requests, the
 * internal frame count can be used to detect when internal captures are
 * submitted due to a lack of available output stream buffers. This situation
 * leads to sensor frames that are not output to the client's output stream,
 * which is usually an undesirable behavior that is referred to as "producer
 * frame drop". This is generally caused by a high consumer processing time,
 * which starves the stream’s available buffer pool, and can often be resolved
 * by decreasing the consumer processing time (reducing the time a buffer is
 * acquired, decreasing system load, increasing hardware clocks, etc.)
 *
 * @defgroup ArgusExtInternalFrameCount Ext::InternalFrameCount
 * @ingroup ArgusExtensions
 */
DEFINE_UUID(ExtensionName, EXT_INTERNAL_FRAME_COUNT, 37afdbda,0020,4f91,957b,46,ea,eb,79,80,c7);

namespace Ext
{

/**
 * @class IInternalFrameCount
 *
 * Interface used to query the internal frame count for a request.
 *
 * Since internal captures do not generate events, detecting internal captures
 * must be done by comparing the internal capture count of successive client-
 * submitted capture requests.
 *
 * This interface is available from:
 *   - CaptureMetadata objects.
 *   - Event objects of type EVENT_TYPE_CAPTURE_STARTED.
 *
 * @ingroup ArgusCaptureMetadata ArgusEventCaptureStarted ArgusExtInternalFrameCount
 */
DEFINE_UUID(InterfaceID, IID_INTERNAL_FRAME_COUNT, c21a7ba2,2b3f,4275,8469,a2,56,34,93,53,93);
class IInternalFrameCount : public Interface
{
public:
    static const InterfaceID& id() { return IID_INTERNAL_FRAME_COUNT; }

    /**
     * Returns the internal frame count for the request.
     */
    virtual uint64_t getInternalFrameCount() const = 0;

protected:
    ~IInternalFrameCount() {}
};

} // namespace Ext

} // namespace Argus

#endif
