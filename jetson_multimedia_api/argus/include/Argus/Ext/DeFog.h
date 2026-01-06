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
 * <b>Libargus Extension: DeFog API</b>
 *
 * @b Description: This file defines the DeFog extension.
 */

#ifndef _ARGUS_DE_FOG_H
#define _ARGUS_DE_FOG_H

namespace Argus
{

/**
 * Adds internal de-fog post-processing algorithms. It introduces one new interface:
 *   - Ext::IDeFogSettings: enables de-fog for a Request.
 *
 * @defgroup ArgusExtDeFog Ext::DeFog
 * @ingroup ArgusExtensions
 */
DEFINE_UUID(ExtensionName, EXT_DE_FOG, 9cf05bd0,1d99,4be8,8732,75,99,55,7f,ed,3a);
namespace Ext
{

/**
 * @class IDeFogSettings
 *
 * Interface to de-fog settings.
 *
 * @ingroup ArgusRequest ArgusExtDeFog
 */
DEFINE_UUID(InterfaceID, IID_DE_FOG_SETTINGS, 9cf05bd1,1d99,4be8,8732,75,99,55,7f,ed,3a);
class IDeFogSettings : public Interface
{
public:
    static const InterfaceID& id() { return IID_DE_FOG_SETTINGS; }

    /**
     * Enables or disables de-fog.
     * @param[in] enable whether or not de-fog is enabled.
     */
    virtual void setDeFogEnable(bool enable) = 0;

    /**
     * @returns whether or not de-fog is enabled.
     */
    virtual bool getDeFogEnable() const = 0;

    /**
     * Sets the amount of fog to be removed. Range 0.0 - 1.0 (none - all).
     * @param[in] amount amount of fog to remove.
     */
    virtual Status setDeFogAmount(float amount) = 0;

    /**
     * @returns the amount of fog to remove.
     */
    virtual float getDeFogAmount() const = 0;

    /**
     * Set the quality of the effect, lower quality results in lower execution time.
     * Range 0.0 - 1.0 (low quality - high quality).
     * @param[in] quality effect quality.
     */
    virtual Status setDeFogQuality(float quality) = 0;

    /**
     * @returns the effect quality.
     */
    virtual float getDeFogQuality() const = 0;

protected:
    ~IDeFogSettings() {}
};

} // namespace Ext

} // namespace Argus

#endif // _ARGUS_DE_FOG_H
