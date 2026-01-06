/*
 * Copyright (c) 2017 NVIDIA Corporation.  All rights reserved.
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

#ifndef ARGUS_SAMPLES_UTILS_RECTUTILS_H_
#define ARGUS_SAMPLES_UTILS_RECTUTILS_H_

#include "Argus/Argus.h"

namespace ArgusSamples
{

namespace RectUtils
{

using namespace Argus;

/**
 * Function to get aspect-ratio-preserving clip rectangle given input and output resolutions.
 * Outputting to a stream with an aspect ratio that does not match the input (sensor) aspect ratio
 * leads to a non-square scaling operation in libargus that should generally be avoided due to
 * performance and image quality reasons. This method will return a clipping rectangle that may be
 * used for the stream settings of a capture request so that the input will be clipped to match the
 * aspect ratio of the output, maintaining square pixels throughout the pipeline.
 * @param[in] inputRes: input resolution
 * @param[in] outputRes: output resolution
 * @returns Rectangle<float>: the aspect-ratio-preserving clip rectangle with either the right
 * and left equally clipped, or the top and bottom equally clipped.  No clipping is done if the
 * input and output aspect ratios are equal.
 */
Rectangle<float> arPreservingClip(const Size2D<uint32_t> &inputRes,
                                  const Size2D<uint32_t> &outputRes);

/**
 * Helper function to get the default clip rectangle.
 */
inline Rectangle<float> defaultClip()
{
    return Rectangle<float>(0.0F, 0.0F, 1.0F, 1.0F);
}

#endif /* ARGUS_SAMPLES_UTILS_RECTUTILS_H_ */

} // namespace RectUtils

} // namespace ArgusSamples
