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

#ifndef ARGUS_APPS_CAMERA_UI_APP_MODULE_VIDEO_H
#define ARGUS_APPS_CAMERA_UI_APP_MODULE_VIDEO_H

#include "IAppModule.h"
#include "tasks/VideoRecord.h"

namespace ArgusSamples
{

/**
 * The video app module adds functionality to record a video and save it.
 */
class AppModuleVideo : public IAppModule, public Window::IKeyObserver
{
public:
    AppModuleVideo();
    virtual ~AppModuleVideo();

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
    static bool video(void *userPtr, const char *optArg);
    static bool toggleRecording(void *userPtr, const char *optArg);
    /**@}*/

private:
    bool m_initialized;                 ///< set if initialized
    bool m_running;                     ///< set if running
    TaskVideoRecord m_videoRecord;      ///< video recording task

    Window::IGuiContainer *m_guiContainerConfig; ///< configuration GUI container
    Window::IGuiContainerGrid *m_guiConfig; ///< configuration GUI

    /** @name IKeyObserver methods */
    /**@{*/
    virtual bool onKey(const Key &key);
    /**@}*/
};

}; // namespace ArgusSamples

#endif // ARGUS_APPS_CAMERA_UI_APP_MODULE_VIDEO_H
