/*
 * Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
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
 * <b>Libargus Extension: Blocking Session Capture Provider API</b>
 *
 * @b Description: This file defines the BlockingSessionCaptureProvider extension.
 */

#ifndef _ARGUS_BLOCKING_SESSION_CAMERA_PROVIDER_H
#define _ARGUS_BLOCKING_SESSION_CAMERA_PROVIDER_H

namespace Argus
{

/**
 * Adds a interface to camera provider to create a blocking capture session.
 * It introduces two new interface:
 *   - Ext::IBlockingSessionCameraProvider: creates blocking capture session.
 *
 * @defgroup ArgusExtBlockingSessionCameraProvider Ext::BlockingSessionCameraProvider
 * @ingroup ArgusExtensions
 */
DEFINE_UUID(ExtensionName, EXT_BLOCKING_SESSION_CAMERA_PROVIDER, 1fff5f04,2ea9,4558,8e92,c2,4b,0b,82,b9,af);


namespace Ext
{

/**
 * @class IBlockingSessionCameraProvider
 *
 * Interface used to create blocking capture session
 *
 * @ingroup ArgusCameraProvider ArgusExtBlockingSessionCameraProvider
 */
DEFINE_UUID(InterfaceID, IID_BLOCKING_SESSION_CAMERA_PROVIDER, 3122fe85,b4cc,4945,af5d,a3,86,26,75,eb,a4);
class IBlockingSessionCameraProvider : public Interface
{
public:
    static const InterfaceID& id() { return IID_BLOCKING_SESSION_CAMERA_PROVIDER; }

    /**
     * Creates and returns a blocking CaptureSession using the given device.
     * For blocking CaptureSession, the capture related API call will block wait until the request
     * is serviced by underlying driver. This will help timing control in client side
     * when client auto control is involved.
     * In compare, for the default CaptureSession, the capture related API call will put
     * the request in a internal queue and return immediately, without blocking client thread.
     * STATUS_UNAVAILABLE will be placed into @c status if the device is already in use.
     * @param[in] device The device to use for the CaptureSession.
     * @param[out] status Optional pointer to return success/status of the call.
     * @returns The new CaptureSession, or NULL if an error occurred.
     */
    virtual CaptureSession* createBlockingCaptureSession(CameraDevice* device,
                                                         Status* status = NULL) = 0;

    /**
     * Creates and returns a blocking CaptureSession using the given device.
     * For blocking CaptureSession, the capture related API call will block wait until the request
     * is serviced by underlying driver. This will help timing control in client side
     * when client auto control is involved.
     * STATUS_UNAVAILABLE will be placed into @c status if any of the devices are already in use.
     * @param[in] devices The device(s) to use for the CaptureSession.
     * @param[out] status Optional pointer to return success/status of the call.
     * @returns The new CaptureSession, or NULL if an error occurred.
     */
    virtual CaptureSession* createBlockingCaptureSession(const std::vector<CameraDevice*>& devices,
                                                         Status* status = NULL) = 0;

protected:
    ~IBlockingSessionCameraProvider() {}
};

} // namespace Ext

} // namespace Argus

#endif

