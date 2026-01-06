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

#include "RectUtils.h"
#include "Error.h"

namespace ArgusSamples
{

namespace RectUtils
{

Rectangle<float> arPreservingClip(const Size2D<uint32_t> &inputRes,
                                  const Size2D<uint32_t> &outputRes)
{
    // Prevent divide-by-zero.
    if (inputRes.width() == 0 || inputRes.height() == 0 ||
        outputRes.width() == 0 || outputRes.height() == 0)
    {
        REPORT_ERROR("Bad params: inputRes[%u x %u], outputRes[%u x %u]",
                inputRes.width(), inputRes.height(), outputRes.width(), outputRes.height());

        return defaultClip();
    }

    const float inputAspectRatio = inputRes.width() / static_cast<float>(inputRes.height());

    const float outputAspectRatio = outputRes.width() / static_cast<float>(outputRes.height());

    // Determine the side and top clips from the sensor and output aspect ratios.
    float sideClip = 0.0F;
    float topClip = 0.0F;

    if (outputAspectRatio < inputAspectRatio)
    {
        // Output is skinnier than input -- clip away sides to preserve square pixels.
        sideClip = (1.0F - outputAspectRatio/inputAspectRatio) / 2.0F;
    }
    else
    {
        // Output is wider or same as input -- clip away top/bottom.
        topClip = (1.0F - inputAspectRatio/outputAspectRatio) / 2.0F;
    }

    return Rectangle<float>(sideClip, topClip, 1.0F - sideClip, 1.0F - topClip);
}

} // namespace RectUtils

} // namespace ArgusSamples



