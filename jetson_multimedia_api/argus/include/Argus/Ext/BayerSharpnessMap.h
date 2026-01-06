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
 * <b>Libargus Extension: Bayer Sharpness Map API</b>
 *
 * @b Description: This file defines the BayerSharpnessMap extension.
 */

#ifndef _ARGUS_EXT_BAYER_SHARPNESS_MAP_H
#define _ARGUS_EXT_BAYER_SHARPNESS_MAP_H

namespace Argus
{

/**
 * Adds internally-generated sharpness metrics to CaptureMetadata results. These are used
 * in order to help determine the correct position of the lens to achieve the best focus.
 * It introduces two new interfaces:
 *   - Ext::IBayerSharpnessMapSettings: enables sharness map generation in a capture Request.
 *   - Ext::IBayerSharpnessMap: exposes the sharpness map metrics from the CaptureMetadata.
 *
 * @defgroup ArgusExtBayerSharpnessMap Ext::BayerSharpnessMap
 * @ingroup ArgusExtensions
 */
DEFINE_UUID(ExtensionName, EXT_BAYER_SHARPNESS_MAP, 7d5e0470,4ea6,11e6,bdf4,08,00,20,0c,9a,66);

namespace Ext
{

/**
 * @class IBayerSharpnessMapSettings
 *
 * Interface to Bayer sharpness map settings.
 *
 * @ingroup ArgusRequest ArgusExtBayerSharpnessMap
 */
DEFINE_UUID(InterfaceID, IID_BAYER_SHARPNESS_MAP_SETTINGS, 7d5e0471,4ea6,11e6,bdf4,08,00,20,0c,9a,66);
class IBayerSharpnessMapSettings : public Interface
{
public:
    static const InterfaceID& id() { return IID_BAYER_SHARPNESS_MAP_SETTINGS; }

    /**
     * Enables or disables Bayer sharpness map generation. When enabled, CaptureMetadata
     * returned by completed captures expose the IBayerSharpnessMap interface.
     * @param[in] enable If True, Bayer sharpness map generation is enabled.
     */
    virtual void setBayerSharpnessMapEnable(bool enable) = 0;

    /**
     * Returns True if sharpness map generation is enabled.
     */
    virtual bool getBayerSharpnessMapEnable() const = 0;

protected:
    ~IBayerSharpnessMapSettings() {}
};

/**
 * @class IBayerSharpnessMap
 *
 * Interface to Bayer sharpness map metadata.
 *
 * The Bayer sharpness map exposes image sharpness metrics that can be used in order
 * to help determine the correct position of the lens to achieve the best focus.
 * Each metric is a normalized floating-point value representing the estimated sharpness
 * for a particular color channel and pixel region, called bins, where 0.0 and 1.0 map to
 * the minimum and maximum possible sharpness values, respectively. Sharpness values
 * generally correlate with image focus in that a low sharpness implies a poorly focused
 * (blurry) region while a high sharpness implies a well focused (sharp) region.
 *
 * The size and layout of the bins used to calculate the sharpness metrics are determined
 * by the libargus implementation, and are illustrated in the following diagram. The bin size
 * and interval are constant across the image, and are positioned such that the generated
 * metrics cover the majority of the full image. All dimensions are given in pixels.
 *
 * @code
 *               start.x                     interval.width
 *               _______                   _________________
 *              |       |                 |                 |
 *           _   ________________________________________________________
 *          |   |                                                        |
 *  start.y |   |                                                        |
 *          |_  |        _____             _____             _____       | _
 *              |       |     |           |     |           |     |      |  |
 *              |       | 0,0 |           | 1,0 |           | 2,0 |      |  |
 *              |       |_____|           |_____|           |_____|      |  |
 *              |                                                        |  | interval.height
 *              |                                                        |  |
 *              |                                                        |  |
 *              |        _____             _____             _____       | _|
 *              |       |     |           |     |           |     |      |
 *              |       | 0,1 |           | 1,1 |           | 2,1 |      |
 *              |       |_____|           |_____|           |_____|      |
 *              |                                                        |
 *              |                                                        |
 *              |                                                        |
 *              |        _____             _____             _____       | _
 *              |       |     |           |     |           |     |      |  |
 *              |       | 0,2 |           | 1,2 |           | 2,2 |      |  | size.height
 *              |       |_____|           |_____|           |_____|      | _|
 *              |                                                        |
 *              |                                                        |
 *              |________________________________________________________|
 *
 *                                                          |_____|
 *
 *                                                         size.width
 * @endcode
 *
 * @ingroup ArgusCaptureMetadata ArgusExtBayerSharpnessMap
 */
DEFINE_UUID(InterfaceID, IID_BAYER_SHARPNESS_MAP, 7d5e0472,4ea6,11e6,bdf4,08,00,20,0c,9a,66);
class IBayerSharpnessMap : public Interface
{
public:
    static const InterfaceID& id() { return IID_BAYER_SHARPNESS_MAP; }

    /**
     * Returns the starting location of the first bin, in pixels, where the
     * location is relative to the top-left corner of the image.
     */
    virtual Point2D<uint32_t> getBinStart() const = 0;

    /**
     * Returns the size of each bin, in pixels.
     */
    virtual Size2D<uint32_t> getBinSize() const = 0;

    /**
     * Returns the number of bins in both the horizontal (width) and vertical (height) directions.
     */
    virtual Size2D<uint32_t> getBinCount() const = 0;

    /**
     * Returns the bin intervals for both the x and y axis. These intervals are defined as the
     * number of pixels between the first pixel of a bin and that of the immediate next bin.
     */
    virtual Size2D<uint32_t> getBinInterval() const = 0;

    /**
     * Returns the sharpness values for all bins and color channels. These values are normalized
     * such that 0.0 and 1.0 map to the minimum and maximum possible sharpness values, respectively.
     *
     * @param[out] values The output array to store the sharpness values for all bins. This
     *             2-dimensional array will be sized as returned by @see getBinCount(), where
     *             each array element is a floating point BayerTuple containing the R, G_EVEN,
     *             G_ODD, and B sharpness values for that bin.
     */
    virtual Status getSharpnessValues(Array2D< BayerTuple<float> >* values) const = 0;

protected:
    ~IBayerSharpnessMap() {}
};

} // namespace Ext

} // namespace Argus

#endif // _ARGUS_EXT_BAYER_SHARPNESS_MAP_H
