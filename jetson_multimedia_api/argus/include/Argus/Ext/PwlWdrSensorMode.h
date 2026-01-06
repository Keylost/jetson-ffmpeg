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
 * <b>Libargus Extension: Piecewise Linear Compression for WDR Sensor Modes</b>
 *
 * @b Description: Adds extra functionalities for the
 * Piecewise Linear (PWL) compressed Wide Dynamic Range (WDR) sensor mode type.
 */

#ifndef _ARGUS_EXT_PWL_WDR_SENSOR_MODE_H
#define _ARGUS_EXT_PWL_WDR_SENSOR_MODE_H

namespace Argus
{

/**
 * Adds extra functionalities for the Piecewise Linear (PWL) Wide Dynamic
 * Range (WDR) sensor mode type. It introduces one new interface:
 *   - Ext::IPwlWdrSensorMode; returns a list of normalized float coordinates (x,y) that define
 *                        the PWL compression curve used in the PWL WDR mode. This PWL compression
 *                        curve is used by the sensor to compress WDR pixel values before sending
 *                        them over CSI. This is done to save bandwidth for data transmission over
 *                        VI-CSI. The compression converts the WDR pixel values from InputBitDepth
 *                        space to OutputBitDepth space.The coordinates of the PWL compression
 *                        curve can be un-normalized by scaling x-axis and y-axis values
 *                        by InputBitDepth and OutputBitDepth respectively. The Bit depths can be
 *                        obtained by using the respective methods in the ISensorMode interface.
 *                        @see ISensorMode
 *
 * @defgroup ArgusExtPwlWdrSensorMode Ext::PwlWdrSensorMode
 * @ingroup ArgusExtensions
 */
DEFINE_UUID(ExtensionName, EXT_PWL_WDR_SENSOR_MODE, 7f510b90,582b,11e6,bbb5,40,16,7e,ab,86,92);

namespace Ext
{

/**
 * @class IPwlWdrSensorMode
 *
 * Interface to the properties of a PWL WDR device.
 *
 * Returns a list of normalized float coordinates (x,y) that define
 * the Piecewise Linear (PWL) compression curve used in the PWL Wide Dynamic Range (WDR) mode.
 * The coordinates are returned in a Point2D tuple. The coordinates
 * can be un-normalized by scaling x-axis and y-axis values by InputBitDepth
 * and OutputBitDepth respectively. The Bit depths can be obtained by using
 * the respective methods in the ISensorMode interface.
 * @see ISensorMode
 *
 * @ingroup ArgusSensorMode ArgusExtPwlWdrSensorMode
 */
DEFINE_UUID(InterfaceID, IID_PWL_WDR_SENSOR_MODE, 7f5acea0,582b,11e6,9414,40,16,7e,ab,86,92);
class IPwlWdrSensorMode : public Interface
{
public:
    static const InterfaceID& id() { return IID_PWL_WDR_SENSOR_MODE; }

    /**
     * Returns the number of control points coordinates in the Piecewise Linear compression
     * curve.
     */
    virtual uint32_t getControlPointCount() const = 0;

    /**
     * Returns the Piecewise Linear (PWL) compression curve coordinates.
     *
     * @param[out] points The output vector to store the PWL compression curve coordinates.
     *             Upon successful return, this vector will filled in with
     *             getControlPointCount() count values, each containing a coordinates of
     *             PWL compression curve within a Point2D tuple.
     */
    virtual Status getControlPoints(std::vector< Point2D<float> >* points) const = 0;

protected:
    ~IPwlWdrSensorMode() {}
};

} // namespace Ext

} // namespace Argus

#endif // _ARGUS_EXT_PWL_WDR_SENSOR_MODE_H
