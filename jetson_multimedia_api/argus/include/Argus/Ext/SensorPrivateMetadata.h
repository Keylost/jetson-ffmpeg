/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
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
 * <b>Libargus Extension: Sensor Private Metadata API</b>
 *
 * @b Description: This file defines the SensorPrivateMetadata extension.
 */

#ifndef _ARGUS_SENSOR_PRIVATE_METADATA_H
#define _ARGUS_SENSOR_PRIVATE_METADATA_H

namespace Argus
{
/**
 * Adds accessors for sensor embedded metadata. This data is metadata that the sensor embeds
 * inside the frame, the type and formating of which depends on the sensor. It is up to the
 * user to correctly parse the data based on the specifics of the sensor used.
 *
 *   - Ext::ISensorPrivateMetadataCaps: Determines whether a device is capable of
 *                                       private metadata output.
 *   - Ext::ISensorPrivateMetadataRequest: Enables private metadata output from a capture request.
 *   - Ext::ISensorPrivateMetadata: Accesses the sensor private metadata.
 *
 * @defgroup ArgusExtSensorPrivateMetadata Ext::SensorPrivateMetadata
 * @ingroup ArgusExtensions
 */
DEFINE_UUID(ExtensionName, EXT_SENSOR_PRIVATE_METADATA, 7acf4352,3a75,46e7,9af1,8d,71,da,83,15,23);

namespace Ext
{

/**
 * @class ISensorPrivateMetadataCaps
 *
 * Interface used to query the availability and size in bytes of sensor private metadata.
 *
 * @ingroup ArgusCameraDevice ArgusExtSensorPrivateMetadata
 */
DEFINE_UUID(InterfaceID, IID_SENSOR_PRIVATE_METADATA_CAPS, e492d2bf,5285,476e,94c5,ee,64,d5,3d,94,ef);
class ISensorPrivateMetadataCaps : public Interface
{
public:
    static const InterfaceID& id() { return IID_SENSOR_PRIVATE_METADATA_CAPS; }

    /**
     * Returns the size in bytes of the private metadata.
     */
    virtual size_t getMetadataSize() const = 0;

protected:
    ~ISensorPrivateMetadataCaps() {}
};

/**
 * @class ISensorPrivateMetadataRequest
 *
 * Interface used enable the output of sensor private metadata for a request.
 *
 * @ingroup ArgusRequest ArgusExtSensorPrivateMetadata
 */
DEFINE_UUID(InterfaceID, IID_SENSOR_PRIVATE_METADATA_REQUEST, 5c868b69,42f5,4ec9,9b93,44,11,c9,6c,02,e3);
class ISensorPrivateMetadataRequest : public Interface
{
public:
    static const InterfaceID& id() { return IID_SENSOR_PRIVATE_METADATA_REQUEST; }

    /**
     * Enables the sensor private metadata, will only work if the sensor supports embedded metadata.
     * @param[in] enable whether to output embedded metadata.
     */
    virtual void setMetadataEnable(bool enable) = 0;

    /**
     * Returns if the metadata is enabled for this request.
     */
    virtual bool getMetadataEnable() const = 0;

protected:
    ~ISensorPrivateMetadataRequest() {}
};

/**
 * @class ISensorPrivateMetadata
 *
 * Interface used to access sensor private metadata.
 *
 * @ingroup ArgusCaptureMetadata ArgusExtSensorPrivateMetadata
 */
DEFINE_UUID(InterfaceID, IID_SENSOR_PRIVATE_METADATA, 68cf6680,70d7,4b52,9a99,33,fb,65,81,a2,61);
class ISensorPrivateMetadata : public Interface
{
public:
    static const InterfaceID& id() { return IID_SENSOR_PRIVATE_METADATA; }

    /**
     * Returns the size of the embedded metadata.
     */
    virtual size_t getMetadataSize() const = 0;

    /**
     * Copies back the metadata to the provided memory location.
     * If the size of @a dst is smaller than the total size of the metadata, only the first
     * bytes up to size are copied.
     * @param [in,out] dst The pointer to the location where the data will be copied.
     *                     The caller is responsible for allocating and managing the memory.
     * @param [in] size The size of the destination.
     */
    virtual Status getMetadata(void *dst, size_t size) const = 0;

protected:
    ~ISensorPrivateMetadata() {}
};

} // namespace Ext

} // namespace Argus

#endif // _ARGUS_SENSOR_PRIVATE_METADATA_H
