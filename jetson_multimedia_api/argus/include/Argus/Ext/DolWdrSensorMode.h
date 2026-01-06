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
 * <b>Libargus Extension: Digital Overlap WDR Sensor Modes</b>
 *
 * @b Description: Adds extra functionalities for the
 * Digital Overlap (DOL) Wide Dynamic Range (WDR) sensor mode type.
 */

#ifndef _ARGUS_EXT_DOL_WDR_SENSOR_MODE_H
#define _ARGUS_EXT_DOL_WDR_SENSOR_MODE_H

namespace Argus
{

/**
 * Adds extra functionalities for the Digital Overlap (DOL) Wide Dynamic
 * Range (WDR) sensor mode type. It introduces one new interface:
 *   - Ext::IDolWdrSensorMode: Returns the extended properties specific to a Digital Overlap (DOL)
 *                        Wide Dynamic Range (WDR) extended sensor mode. DOL WDR is a
 *                        multi-exposure technology that enables fusion of various exposures
 *                        from a single frame to produce a WDR image.
 *
 * @defgroup ArgusExtDolWdrSensorMode Ext::DolWdrSensorMode
 * @ingroup ArgusExtensions
 */
DEFINE_UUID(ExtensionName, EXT_DOL_WDR_SENSOR_MODE, 569fb210,70d9,11e7,9598,08,00,20,0c,9a,66);

namespace Ext
{

/**
 * @class IDolWdrSensorMode
 *
 * Interface to the properties of a DOL WDR device.
 *
 * Returns the extended properties specific to a Digital Overlap (DOL)
 * Wide Dynamic Range (WDR) extended sensor mode. DOL WDR is a multi-exposure technology
 * that enables fusion of various exposures from a single frame to produce a WDR image.
 *
 * A DOL WDR RAW buffer contains different DOL exposures in an interleaved layout. DOL WDR
 * supports two exposure (long and short) and three exposure (long, short and very short) schemes.
 * These schemes are referred to as DOL-2 and DOL-3 respectively.
 *
 * Exposures are time staggered which leads to vertical blank period (VBP) rows being inserted
 * in between various exposures. This scheme results in (N-1) sections of VBP rows for an N exposure
 * DOL WDR frame.
 *
 * Each exposure is preceded by optical black (OB) rows.
 *
 * Each row of DOL WDR RAW interleaved frame starts with a few Line Info (LI) marker pixels.
 * LI pixels distinguish the kind of row.
 * Row types include:
 * a. Long Exposure
 * b. Short Exposure
 * c. Very Short Exposure
 * d. Vertical Blank Period
 *
 * For a DOL-2 exposure scheme, there is only one section of VBP rows. The data layout per exposure
 * looks like this:
 * Long exposure  has OB rows, image rows, VBP rows.
 * Short exposure has OB rows, VBP rows,   image rows.
 *
 * The ordering of VBP rows changes across exposures but the count of VBP rows per exposure
 * remains the same. The final interleaved DOL WDR RAW frame buffer is produced by interleaving
 * each exposure's data on a per row basis in a round robin fashion across exposures.
 *
 * For a DOL-3 exposure scheme, there are two sections of VBP rows. For the sake of terminology
 * these are referred to as VBP[0] and VBP[1]. The data layout per exposure looks like this:
 * Long exposure       has OB rows, image rows,   VBP[0] rows,  VBP[1] rows.
 * Short exposure      has OB rows, VBP[0] rows,  image rows,   VBP[1] rows.
 * Very Short exposure has OB rows, VBP[0] rows,  VBP[1] rows,  image rows.
 *
 * Again, only the ordering of VBP[0] and VBP[1] rows changes across exposures but the count of
 * VBP[0] and VBP[1] rows remains the same. Similar to the DOL-2 scheme, the final interleaved
 * DOL WDR RAW frame buffer for DOL-3 scheme is produced by interleaving each exposure's data
 * on a per row basis in a round robin fashion across exposures.
 *
 * This scheme can be extended to DOL-N exposures with (N-1) sections of VBP rows ranging from
 * VBP[0] to VBP[N-2]. When considering the vertical blank period sections for exposure N,
 * the rows of VBP[X] will come before the image data if X < N, otherwise they will come
 * after the image data.
 *
 * Hence, a DOL-N RAW buffer would have different dimensions than the fused output
 * WDR frame buffer. The resolution of the DOL-N RAW buffer is referred to as physical resolution.
 *
 * The set of properties for basic sensor modes is still applicable to DOL WDR sensor mode. Those
 * properties are available through the ISensorMode interface. The only difference is that the
 * resolution property provided by the ISensorMode interface for DOL WDR would be the size of the
 * fused WDR frame. WDR fusion typically eliminates LI markers, OB rows and VBP rows and merges the
 * individual exposures to create a frame that is smaller in height and width than the
 * DOL WDR RAW interleaved frame.
 *
 * Following the LI marker pixels is the actual pixel data for each row. This data may include
 * margin pixels on the left or right side of the row, which are generally used for filtering
 * and cropped out of a fused DOL image. The width of these margin pixels can be queried by
 * getLeftMarginWidth()/getRightMarginWidth().
 * @see ISensorMode
 *
 * @ingroup ArgusSensorMode ArgusExtDolWdrSensorMode
 */
DEFINE_UUID(InterfaceID, IID_DOL_WDR_SENSOR_MODE, a1f4cae0,70dc,11e7,9598,08,00,20,0c,9a,66);
class IDolWdrSensorMode : public Interface
{
public:
    static const InterfaceID& id() { return IID_DOL_WDR_SENSOR_MODE; }

    /**
     * Returns the number of exposures captured per frame for this DOL WDR mode.
     * Typically, 2 = Long, Short or 3 = Long, Short, Very Short exposures.
     */
    virtual uint32_t getExposureCount() const = 0;

    /**
     * Returns number of Optical Black rows at the start of each exposure in a DOL WDR frame.
     */
    virtual uint32_t getOpticalBlackRowCount() const = 0;

    /**
     * Returns number of vertical blank period rows for each DOL WDR exposure.
     *
     * @param[out] verticalBlankPeriodRowCounts The output vector to store the
     *             vertical blank period (VBP) rows per DOL WDR exposure. Size of the vector is
     *             getExposureCount()-1 count values. When considering the vertical blank period
     *             sections for exposure N, the rows of VBP[X] will come before the image data
     *             if X < N, otherwise they will come after the image data.
     */
    virtual Status getVerticalBlankPeriodRowCount(
            std::vector<uint32_t>* verticalBlankPeriodRowCounts) const = 0;

    /**
     * Returns line info markers width in pixels.
     * These occur at the start of each pixel row to distinguish row types. There are different
     * line info markers to distinguish each different exposure and vertical blank period rows.
     *
     * Optical black rows have the same line info markers as the exposure type they appear on.
     */
    virtual uint32_t getLineInfoMarkerWidth() const = 0;

    /**
     * Returns number of margin pixels on left per row.
     */
    virtual uint32_t getLeftMarginWidth() const = 0;

    /**
     * Returns number of margin pixels on right per row.
     */
    virtual uint32_t getRightMarginWidth() const = 0;

    /**
     * Returns the physical resolution derived due to the interleaved exposure output from DOL WDR
     * frames.
     */
    virtual Size2D<uint32_t> getPhysicalResolution() const = 0;

protected:
    ~IDolWdrSensorMode() {}
};

} // namespace Ext

} // namespace Argus

#endif // _ARGUS_EXT_DOL_WDR_SENSOR_MODE_H
