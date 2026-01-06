/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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
 * <b>Libargus Extension: Sensor Private Metadata Client Buffer API</b>
 *
 * @b Description: This file defines the SensorPrivateMetadataClientBuffer extension.
 */

#ifndef _ARGUS_SENSOR_PRIVATE_METADATA_CLIENT_BUFFER_H
#define _ARGUS_SENSOR_PRIVATE_METADATA_CLIENT_BUFFER_H

namespace Argus
{
/**
 * Adds accessors for set client buffer for sensor embedded metadata. Sensor embeds private info
 * (for example PDAF data) in sensor metadata. Client using Ext::SensorPrivateMetadata API
 * to obtain this meta data involves several memcpy. When sensor metadata size is large,
 * this will cause high CPU usage and affect camera performance.
 * ISensorPrivateMetadataClientBufferRequest allow client to set a client buffer and Argus write
 * to it directly without extra memcpy.
 * This is only supported in single process mode as in client-server (multiprocess) mode, client
 * and server are in different process and their own address space.
 *
 *   - Ext::ISensorPrivateMetadataClientBufferRequest: Sets client buffer for private metadata.
 *
 * @defgroup ArgusExtSensorPrivateMetadataClientBuffer Ext::SensorPrivateMetadataClientBuffer
 * @ingroup ArgusExtensions
 */
DEFINE_UUID(ExtensionName, EXT_SENSOR_PRIVATE_METADATA_CLIENT_BUFFER, 85cbb9b6,cd7f,4e8c,9462,9f,21,cd,a7,40,1c);

namespace Ext
{

/**
 * @class ISensorPrivateMetadataClientBufferRequest
 *
 * Interface used to set client buffer for sensor private metadata for a request
 *
 * @ingroup ArgusRequest ArgusExtSensorPrivateMetadataClientBuffer
 */
DEFINE_UUID(InterfaceID, IID_SENSOR_PRIVATE_METADATA_CLIENT_BUFFER_REQUEST, 5c868b69,42f5,4ec9,9b93,44,11,c9,6c,02,e4);
class ISensorPrivateMetadataClientBufferRequest : public Interface
{
public:
    static const InterfaceID& id() { return IID_SENSOR_PRIVATE_METADATA_CLIENT_BUFFER_REQUEST; }

    /**
     * Client allocate the metadata buffer and set the address of the buffer,
     * Argus writes the sensor metadata directly to the buffer.
     * This method is supported only in single process mode.
     * @param[in] buf  Specifies the address of client buffer.
     * @param[in] size Specifies the size in bytes of the metadata buffer.
     */
    virtual Status setClientMetadataBuffer(void* buf, size_t size) = 0;

    /**
     * Returns if client metadata buffer is used for this request.
     */
    virtual bool getClientMetadataBufferEnable() const = 0;

protected:
    ~ISensorPrivateMetadataClientBufferRequest() {}
};

} // namespace Ext

} // namespace Argus

#endif // _ARGUS_SENSOR_PRIVATE_METADATA_CLIENT_BUFFER_H
