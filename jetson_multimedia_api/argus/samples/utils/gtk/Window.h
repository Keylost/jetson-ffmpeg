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

#ifndef ARGUS_SAMPLES_UTILS_GTK_WINDOW_H
#define ARGUS_SAMPLES_UTILS_GTK_WINDOW_H

#include "WindowBase.h"

/// The window supports GTK
#define WINDOW_GUI_SUPPORT WINDOW_GUI_GTK

namespace ArgusSamples
{

/**
 * GTK implementation of the window class
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
    virtual bool setWindowGui(IGuiContainer *iGuiContainer = NULL,
        IGuiElement *iGuiElementWindow = NULL, IGuiElement *iGuiElementView = NULL);
    /**@}*/

    /**
     * A GUI builder is a special container initialized with a builder string.
     */
    class IGuiBuilder : public IGuiContainer
    {
    public:
        virtual ~IGuiBuilder() {}

        /**
         * Create a GUI builder using 'builderString'.
         *
         * @param builderString [in]    GTK builder string
         * @param iGuiBuilder [out]   the created GUI builder
         */
        static bool create(const char *builderString, IGuiBuilder **iGuiBuilder);

        /**
         * Create an element with 'name' from the builder. This creates a new IGuiElement
         * referencing the element in the builder. The caller is responsible to destroy the new
         * IGuiElement when it's no longer used.
         */
        virtual IGuiElement *createElement(const char *name) = 0;
    };

private:
    Window();
    virtual ~Window();

    // this is a singleton, hide copy constructor etc.
    Window(const Window&);
    Window& operator=(const Window&);

    virtual bool initialize();

    // Allow GTK call backs to access private data
    class gtkCallBacks;
    friend class gtkCallBacks;

    // Allow signal call backs to access private data
    class SignalCallbacks;
    friend class SignalCallBacks;

    class PrivateData;
    PrivateData *m_private;     ///< hide private data
};

}; // namespace ArgusSamples

#endif // ARGUS_SAMPLES_UTILS_GTK_WINDOW_H
