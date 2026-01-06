/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
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

#ifndef ARGUS_APPS_CAMERA_UI_COMMON_APP_MODULE_H
#define ARGUS_APPS_CAMERA_UI_COMMON_APP_MODULE_H

#include "Window.h"

namespace ArgusSamples
{

class Options;

/**
 * A application module exposes command line options, GUI elements and executes tasks.
 */
class IAppModule
{
public:
    IAppModule() { }
    virtual ~IAppModule() { };

    /**
     * Initialize
     *
     * @param [in] options  each app module can add options
     */
    virtual bool initialize(Options &options) = 0;

    /**
     * Shut down
     */
    virtual bool shutdown() = 0;

    /**
     * Start the module
     *
     * @param [in] iGuiMenuBar  optional, the module can add menu items to this menu bar
     * @param [in] iGuiContainerConfig  optional, the module can add configuration options to this
     *                                  container
     */
    virtual bool start(Window::IGuiMenuBar *iGuiMenuBar = NULL,
        Window::IGuiContainer *iGuiContainerConfig = NULL) = 0;

    /**
     * Stop the module
     */
    virtual bool stop() = 0;
};

}; // namespace ArgusSamples

#endif // ARGUS_APPS_CAMERA_UI_COMMON_APP_MODULE_H
