/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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
 * <b>Libargus Extension: TSC HW SensorTimestamp API</b>
 *
 * @b Description: This file defines the SensorTimestampTsc extension.
 */

#ifndef _ARGUS_SENSOR_TIMESTAMP_TSC_H
#define _ARGUS_SENSOR_TIMESTAMP_TSC_H

namespace Argus
{

/**
 * Adds a timestamp interface to get TSC HW timestamp.
 * It introduces one new interface:
 *   - Ext::ISensorTimestampTsc: gets TSC HW timestamp.
 * @defgroup ArgusExtSensorTimestampTsc Ext::SensorTimestampTsc
 * @ingroup ArgusExtensions
 */
DEFINE_UUID(ExtensionName, EXT_SENSOR_TIMESTAMP_TSC, e6cc1360,06ea,11eb,8b6e,08,00,20,0c,9a,66);


namespace Ext
{

/**
 * @class ISensorTimestampTsc
 *
 * Interface used to get TSC HW timestamp
 *
 * @ingroup ArgusCaptureMetadata ArgusExtSensorTimestampTsc
 */
DEFINE_UUID(InterfaceID, IID_SENSOR_TIMESTAMP_TSC, 35581ba0,06eb,11eb,8b6e,08,00,20,0c,9a,66);
class ISensorTimestampTsc : public Interface
{
public:
    static const InterfaceID& id() { return IID_SENSOR_TIMESTAMP_TSC; }

    /**
     * Returns the VI HW (SOF) timestamp based on tegra wide timestamp system counter (TSC)
     * This is the start timestamp for the sensor (in nanoseconds).
     */
    virtual uint64_t getSensorSofTimestampTsc() const = 0;

    /**
     * Returns the VI HW (EOF) timestamp based on tegra wide timestamp system counter (TSC)
     * This is the end timestamp for the sensor (in nanoseconds).
     */
    virtual uint64_t getSensorEofTimestampTsc() const = 0;

protected:
    ~ISensorTimestampTsc() {}
};

} // namespace Ext

} // namespace Argus

#endif
