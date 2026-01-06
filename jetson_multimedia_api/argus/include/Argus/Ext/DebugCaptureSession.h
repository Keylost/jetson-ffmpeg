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
 * <b>Libargus Extension: Debug Capture Session API</b>
 *
 * @b Description: This file defines the DebugCaptureSession extension.
 */

#ifndef _ARGUS_DEBUG_CAPTURE_SESSION_H
#define _ARGUS_DEBUG_CAPTURE_SESSION_H

namespace Argus
{

enum ArgusInjectError
{
    ArgusInjectError_ErrorLongLine = 1
};

/**
 * Adds a debug interface to dump internal libargus runtime information.
 * It introduces one new interface:
 *   - Ext::IDebugCaptureSession: dumps session runtime information.
 *
 * @defgroup ArgusExtDebugCaptureSession Ext::DebugCaptureSession
 * @ingroup ArgusExtensions
 */
DEFINE_UUID(ExtensionName, EXT_DEBUG_CAPTURE_SESSION, 1fee5f03,2ea9,4558,8e92,c2,4b,0b,82,b9,af);


namespace Ext
{

/**
 * @class IDebugCaptureSession
 *
 * Interface used to dump CaptureSession runtime information
 *
 * @ingroup ArgusCaptureSession ArgusExtDebugCaptureSession
 */
DEFINE_UUID(InterfaceID, IID_DEBUG_CAPTURE_SESSION, 2122fe84,b4cc,4945,af5d,a3,86,26,75,eb,a4);
class IDebugCaptureSession : public Interface
{
public:
    static const InterfaceID& id() { return IID_DEBUG_CAPTURE_SESSION; }

    /**
     * Returns session runtime information to the specified file descriptor.
     */
    virtual Status dump(int32_t fd) const = 0;

    /**
     * Set event injection error id.
     */
    virtual Status setEventInjectionErrorMsg(ArgusInjectError errorId) = 0;

protected:
    ~IDebugCaptureSession() {}
};

} // namespace Ext

} // namespace Argus

#endif

