/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION. All rights reserved.
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

#ifndef WINDOW_H
#define WINDOW_H

#include "WindowBase.h"

/// The window supports GLX
#define WINDOW_GUI_SUPPORT WINDOW_GUI_GLX

namespace ArgusSamples
{

/**
 * GLX implementation of the window class
 */
class Window : public WindowBase
{
public:
    /**
     * Get the window instance.
     */
    static Window &getInstance();

    /** @name WindowBase methods */
    /**@{*/
    virtual bool shutdown();
    virtual bool pollEvents();
    virtual bool eventLoop();
    virtual bool requestExit();
    virtual EGLNativeDisplayType getEGLNativeDisplay() const;
    virtual EGLNativeWindowType getEGLNativeWindow() const;
    virtual uint32_t getWidth() const;
    virtual uint32_t getHeight() const;
    virtual bool setWindowRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    using WindowBase::setWindowRect;
    /**@}*/

private:
    Window();
    virtual ~Window();

    // this is a singleton, hide copy constructor etc.
    Window(const Window&);
    Window& operator=(const Window&);

    virtual bool initialize();

    class PrivateData;
    PrivateData *m_private;     ///< hide private data
};

}; // namespace ArgusSamples

#endif // WINDOW_H
