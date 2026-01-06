/*
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
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
 * <b>Libargus Extension: Non Linear Histogram</b>
 *
 * @b Description: This file defines the Non Linear Histogram extension, and provide
 * methods to interpret non linear data in case of compressed data
 */

#ifndef _ARGUS_NON_LINEAR_HISTOGRAM_H
#define _ARGUS_NON_LINEAR_HISTOGRAM_H

namespace Argus
{

/**
 * This adds a method to interpret the compressed histogram data correctly
 * It introduces one new interface:
 *  - Ext::INonLinearHistogram: returns a list of normalized bin indices that map bins from
 *                        getHistogram() to compressed pixel values.
 * @defgroup ArgusExtNonLinearHistogram Ext::NonLinearHistogram
 * @ingroup ArgusExtensions
 */
DEFINE_UUID(ExtensionName, EXT_NON_LINEAR_HISTOGRAM, 45b6a850,e801,11e8,b568,08,00,20,0c,9a,66);

namespace Ext
{

/**
 * @class INonLinearHistogram
 *
 * Interface used to query the metadata to correctly interpret the compressed histogram data.
 * Returns the normalized bin values to correctly interpret the compressed bayer histogram
 * data. This interface will only be exposed in case the histogram is compressed.
 *
 * This interface is available from:
 *   - Histogram child objects returned by ICaptureMetadata::getBayerHistogram()
 *
 * @ingroup ArgusCaptureMetadata ArgusExtNonLinearHistogram
 */
DEFINE_UUID(InterfaceID, IID_NON_LINEAR_HISTOGRAM, 6e337ec0,e801,11e8,b568,08,00,20,0c,9a,66);
class INonLinearHistogram : public Interface
{
public:
    static const InterfaceID& id() { return IID_NON_LINEAR_HISTOGRAM; }

    /**
     * Returns the average bayer values of bins for bayer histogram data.
     *
     * @param[out] binValues Returns the normalized average bin values (float in [0,1]) for
     *             bins provided by IBayerHistogram interface.
     *             In case the histogram data provided by IBayerHistogram::getHistogram()
     *             is non-linear, this method will return a vector having the same size as
     *             histogram (i.e. IBayerHistogram::getBinCount()), and will contain
     *             normalized bayer colour values to which the histogram bin of the same
     *             index corresponds.
     *
     *             For Example, in case of Non Linear Histogram
     *
     *             IBayerHistogram->getHistogram(&histogram);
     *             INonLinearHistogram->getBinValues(&values);
     *
     *             for(int i = 0 ; i < histogram.size() ; i++)
     *             {
     *                  cout<<" bin: " << i
     *                      <<" normalized bin Value: " << values[i]
     *                      <<" frequency: " << histogram[i];
     *             }
     */
    virtual Status getHistogramBinValues(std::vector< BayerTuple<float> >* binValues) const = 0;

protected:
    ~INonLinearHistogram() {}
};

} // namespace Ext

} // namespace Argus

#endif // _ARGUS_NON_LINEAR_HISTOGRAM_H
