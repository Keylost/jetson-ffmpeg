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
 * <b>Libargus API: Camera Provider API</b>
 *
 * @b Description: This file defines the CameraProvider object and interface.
 */

#ifndef _ARGUS_CAMERA_PROVIDER_H
#define _ARGUS_CAMERA_PROVIDER_H

namespace Argus
{

/**
 * Object providing the entry point to the libargus runtime.
 *
 * It provides methods for querying the cameras in the system and for
 * creating camera devices.
 *
 * @defgroup ArgusCameraProvider CameraProvider
 * @ingroup ArgusObjects
 */
class CameraProvider : public InterfaceProvider, public Destructable
{
public:

    /**
     * Creates and returns a new CameraProvider.
     * If a CameraProvider object has already been created,
     * this method will return a pointer to that object.
     *
     * @param[out] status Optional pointer to return success/status of the call.
     */
    static CameraProvider* create(Status* status = NULL);

protected:
    ~CameraProvider() {}
};

/**
 * @class ICameraProvider
 *
 * Interface to the core CameraProvider methods.
 *
 * @ingroup ArgusCameraProvider
 */
DEFINE_UUID(InterfaceID, IID_CAMERA_PROVIDER, a00f33d7,8564,4226,955c,2d,1b,cd,af,a3,5f);

class ICameraProvider : public Interface
{
public:
    static const InterfaceID& id() { return IID_CAMERA_PROVIDER; }

    /**
     * Returns the version number of the libargus implementation. This string will begin with
     * the major and minor version numbers, separated by a period, and may be followed by
     * any additional vendor-specific version information.
     */
    virtual const std::string& getVersion() const = 0;

    /**
     * Returns the vendor string for the libargus implementation.
     */
    virtual const std::string& getVendor() const = 0;

    /**
     * Returns whether or not an extension is supported by this libargus implementation.
     * This is generally used during process initialization to ensure that all required
     * extensions are present before initializing any CaptureSessions. Note, however,
     * that having an extension be supported does not imply that the resources or
     * devices required for that extension are available; standard interface checking
     * and any other extension-specific runtime checks, as described by the extension
     * documentation, should always be performed before any extension is used.
     * @param[in] extension the extension identifier.
     */
    virtual bool supportsExtension(const ExtensionName& extension) const = 0;

    /**
     * Returns the list of camera devices that are exposed by the provider. This
     * includes devices that may already be in use by active CaptureSessions, and
     * it's the application's responsibility to check device availability and/or
     * handle any errors returned when CaptureSession creation fails due to a
     * device already being in use.
     * @param[out] devices A vector that will be populated by the available devices.
     *
     * @returns success/status of the call.
     */
    virtual Status getCameraDevices(std::vector<CameraDevice*>* devices) const = 0;

    /**
     * Creates and returns a new CaptureSession using the given device.
     * STATUS_UNAVAILABLE will be placed into @c status if the device is already in use.
     * @param[in] device The device to use for the CaptureSession.
     * @param[out] status Optional pointer to return success/status of the call.
     * @returns The new CaptureSession, or NULL if an error occurred.
     */
    virtual CaptureSession* createCaptureSession(CameraDevice* device,
                                                 Status* status = NULL) = 0;

    /**
     * Creates and returns a new CaptureSession using the given device(s).
     * STATUS_UNAVAILABLE will be placed into @c status if any of the devices are already in use.
     * @param[in] devices The device(s) to use for the CaptureSession.
     * @param[out] status Optional pointer to return success/status of the call.
     * @returns The new CaptureSession, or NULL if an error occurred.
     */
    virtual CaptureSession* createCaptureSession(const std::vector<CameraDevice*>& devices,
                                                 Status* status = NULL) = 0;

protected:
    ~ICameraProvider() {}
};

} // namespace Argus

#endif // _ARGUS_CAMERA_PROVIDER_H
