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
 * <b>Libargus Extension: Bayer Average Map API</b>
 *
 * @b Description: This file defines the BayerAverageMap extension.
 */

#ifndef _ARGUS_EXT_BAYER_AVERAGE_MAP_H
#define _ARGUS_EXT_BAYER_AVERAGE_MAP_H

namespace Argus
{

/**
 * Generates local averages of a capture's raw Bayer data. These averages are generated for
 * a number of small rectangles, called bins, that are evenly distributed across the image.
 * These averages may be calculated before optical clipping to the output bit depth occurs, thus
 * the working range of this averaging may extend beyond the optical range of the output pixels;
 * this allows the averages to remain steady while the sensor freely modifies its optical range.
 *
 * Any pixel values outside of the working range are clipped with respect to this averaging.
 * Specifically, the API excludes them from the average calculation and increments
 * the clipped pixel counter for the containing region.
 * @see Ext::IBayerAverageMap::getClipCounts()
 *
 * This extension introduces two new interfaces:
 *   - Ext::IBayerAverageMapSettings enables average map generation in a capture Request.
 *   - Ext::IBayerAverageMap exposes the average map values from the CaptureMetadata.
 *
 * @defgroup ArgusExtBayerAverageMap Ext::BayerAverageMap
 * @ingroup ArgusExtensions
 */
DEFINE_UUID(ExtensionName, EXT_BAYER_AVERAGE_MAP, 12c3de20,64c5,11e6,bdf4,08,00,20,0c,9a,66);

namespace Ext
{

/**
 * @class IBayerAverageMapSettings
 *
 * Interface to Bayer average map settings.
 *
 * @ingroup ArgusRequest ArgusExtBayerAverageMap
 */
DEFINE_UUID(InterfaceID, IID_BAYER_AVERAGE_MAP_SETTINGS, 12c3de21,64c5,11e6,bdf4,08,00,20,0c,9a,66);
class IBayerAverageMapSettings : public Interface
{
public:
    static const InterfaceID& id() { return IID_BAYER_AVERAGE_MAP_SETTINGS; }

    /**
     * Enables or disables Bayer average map generation. When enabled, CaptureMetadata
     * returned by completed captures will expose the IBayerAverageMap interface.
     *
     * @param[in] enable whether or not Bayer average map generation is enabled.
     */
    virtual void setBayerAverageMapEnable(bool enable) = 0;

    /**
     * @returns whether or not Bayer average map generation is enabled.
     */
    virtual bool getBayerAverageMapEnable() const = 0;

protected:
    ~IBayerAverageMapSettings() {}
};

/**
 * @class IBayerAverageMap
 *
 * Interface to Bayer average map metadata.
 *
 * The Bayer average map provides local averages of the capture's raw pixels for a number
 * of small rectangular regions, called bins, that are evenly distributed across the image.
 * Each average is a floating-point value that is nomalized such that [0.0, 1.0] maps to the
 * full optical range of the output pixels, but values outside this range may be included in
 * the averages so long as they are within the working range of the average calculation.
 * For pixels that have values outside the working range, the API excludes such pixels from the
 * average calculation and increments the clipped pixel counter for the containing region.
 * @see IBayerAverageMap::getWorkingRange()
 * @see IBayerAverageMap::getClipCounts()
 *
 * The size and layout of the bins used to calculate the averages are determined by the Argus
 * implementation and are illustrated in the following diagram. The bin size and interval are
 * constant across the image, and are positioned such that the generated averages cover the
 * majority of the full image. All dimensions are given in pixels.
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
 * @ingroup ArgusCaptureMetadata ArgusExtBayerAverageMap
 */
DEFINE_UUID(InterfaceID, IID_BAYER_AVERAGE_MAP, 12c3de22,64c5,11e6,bdf4,08,00,20,0c,9a,66);
class IBayerAverageMap : public Interface
{
public:
    static const InterfaceID& id() { return IID_BAYER_AVERAGE_MAP; }

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
     * This size is equivalent to the array dimensions for the output from
     * IBayerAverageMap::getAverages() or IBayerAverageMap::getClipCounts().
     */
    virtual Size2D<uint32_t> getBinCount() const = 0;

    /**
     * Returns the bin intervals for both the x and y axis. These intervals are defined as the
     * number of pixels between the first pixel of a bin and that of the immediate next bin.
     */
    virtual Size2D<uint32_t> getBinInterval() const = 0;

    /**
     * Returns the working range of the averaging calculation. The working range is defined as
     * the range of values that are included in the average calculation (e.g. not clipped),
     * and may extend beyond the normalized [0.0, 1.0] range of the optical output. For example,
     * if the working range is [-0.5, 1.5], this means that values in [-0.5, 0) and (1, 1.5] will
     * still be included in the average calculation despite being clipped to [0.0, 1.0] in the
     * output pixels. Any pixels outside this working range are excluded from average calculation
     * and will increment the clip count.
     * @see IBayerAverageMap::getClipCounts()
     *
     * @note When the bit depth available for averaging is equal to the optical bit depth of
     * the output, the working range will be less than the full [0.0, 1.0] optical range. For
     * example, when 10 bits of data are available, the raw output pixels in [0u, 1023u] will
     * map to [0.0, 1.0]; however, the values of 0 and 1023 will be considered clipped for the
     * sake of average calculation, and so the working range would be [1/1023.0, 1022/1023.0].
     */
    virtual Range<float> getWorkingRange() const = 0;

    /**
     * Returns the average values for all bins. These values are normalized such that
     * [0.0, 1.0] maps to the optical range of the output, but the range of possible values
     * is determined by the working range. For input pixels that have values outside the
     * working range, the API excludes such pixels from the average calculation and
     * increments the clipped pixel counter for the containing region.
     * @see IBayerAverageMap::getWorkingRange()
     * @see IBayerAverageMap::getClipCounts()
     *
     * @param[out] averages The output array to store the averages for all bins. This
     *             2-dimensional array is sized as returned by IBayerAverageMap::getBinCount(),
     *             where each array element is a floating point BayerTuple containing the R,
     *             G_EVEN, G_ODD, and B averages for that bin.
     */
    virtual Status getAverages(Array2D< BayerTuple<float> >* averages) const = 0;

    /**
     * Returns the clipped pixel counts for all bins. This is the number of pixels in the bin
     * whose value exceeds the working range and have been excluded from average calculation.
     * @see IBayerAverageMap::getWorkingRange()
     *
     * @param[out] clipCounts The output array to store the clip counts for all bins. This
     *             2-dimensional array is sized as returned by
     *             Ext::IBayerAverageMap::getBinCount(), where each array element is an uint32_t
     *             BayerTuple containing the R, G_EVEN, G_ODD, and B clip counts for that bin.
     */
    virtual Status getClipCounts(Array2D< BayerTuple<uint32_t> >* clipCounts) const = 0;

protected:
    ~IBayerAverageMap() {}
};

} // namespace Ext

} // namespace Argus

#endif // _ARGUS_EXT_BAYER_AVERAGE_MAP_H
