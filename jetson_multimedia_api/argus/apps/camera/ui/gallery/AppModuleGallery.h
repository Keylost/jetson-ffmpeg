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

#ifndef ARGUS_APPS_CAMERA_UI_APP_MODULE_GALLERY_H
#define ARGUS_APPS_CAMERA_UI_APP_MODULE_GALLERY_H

#include "IAppModule.h"
#include "tasks/Gallery.h"

namespace ArgusSamples
{

/**
 * The gallery app module adds functionality to review images and videos.
 */
class AppModuleGallery : public IAppModule, public Window::IKeyObserver
{
public:
    AppModuleGallery();
    virtual ~AppModuleGallery();

    /** @name IAppModule methods */
    /**@{*/
    virtual bool initialize(Options &options);
    virtual bool shutdown();
    virtual bool start(Window::IGuiMenuBar *iGuiMenuBar = NULL,
        Window::IGuiContainer *iGuiContainerConfig = NULL);
    virtual bool stop();
    /**@}*/

    /** @name option callbacks */
    /**@{*/
    static bool prevItem(void *userPtr, const char *optArg);
    static bool nextItem(void *userPtr, const char *optArg);
    static bool togglePlayBack(void *userPtr, const char *optArg);
    static bool rewind(void *userPtr, const char *optArg);
    /**@}*/

private:
    bool m_initialized;         ///< set if initialized
    bool m_running;             ///< set if running
    TaskGallery m_gallery;      ///< gallery task

    Window::IGuiContainer *m_guiContainerConfig; ///< configuration GUI container
    Window::IGuiContainerGrid *m_guiConfig; ///< configuration GUI

    /** @name IKeyObserver methods */
    /**@{*/
    virtual bool onKey(const Key &key);
    /**@}*/
};

}; // namespace ArgusSamples

#endif // ARGUS_APPS_CAMERA_UI_APP_MODULE_GALLERY_H
