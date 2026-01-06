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

#include "AppModuleMultiSession.h"

#include <stdlib.h>

#include "Options.h"
#include "Error.h"

namespace ArgusSamples
{

AppModuleMultiSession::AppModuleMultiSession()
    : m_initialized(false)
{
}

AppModuleMultiSession::~AppModuleMultiSession()
{
    shutdown();
}

bool AppModuleMultiSession::initialize(Options &options)
{
    if (m_initialized)
        return true;

    PROPAGATE_ERROR(m_multiSession.initialize());

    PROPAGATE_ERROR(options.addOption(
        createValueOption("multidevices", 0, "INDEX", "select multiple camera devices with INDEX.",
            m_multiSession.m_multiDevices)));

    m_initialized = true;

    return true;
}

bool AppModuleMultiSession::shutdown()
{
    if (!m_initialized)
        return true;

    PROPAGATE_ERROR_CONTINUE(m_multiSession.shutdown());

    m_initialized = false;
    return true;
}

bool AppModuleMultiSession::start(Window::IGuiMenuBar *iGuiMenuBar,
    Window::IGuiContainer *iGuiContainerConfig)
{
    PROPAGATE_ERROR(m_multiSession.start());
    return true;
}

bool AppModuleMultiSession::stop()
{
    PROPAGATE_ERROR(m_multiSession.stop());
    return true;
}

}; // namespace ArgusSamples
