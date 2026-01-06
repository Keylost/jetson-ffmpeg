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

#include <stdlib.h>

#include "AppModuleGallery.h"
#include "Error.h"

namespace ArgusSamples
{

/* static */ bool AppModuleGallery::prevItem(void *userPtr, const char *optArg)
{
    AppModuleGallery *module = reinterpret_cast<AppModuleGallery*>(userPtr);

    PROPAGATE_ERROR(module->m_gallery.prevItem());

    return true;
}

/* static */ bool AppModuleGallery::nextItem(void *userPtr, const char *optArg)
{
    AppModuleGallery *module = reinterpret_cast<AppModuleGallery*>(userPtr);

    PROPAGATE_ERROR(module->m_gallery.nextItem());

    return true;
}

/* static */ bool AppModuleGallery::togglePlayBack(void *userPtr, const char *optArg)
{
    AppModuleGallery *module = reinterpret_cast<AppModuleGallery*>(userPtr);

    PROPAGATE_ERROR(module->m_gallery.togglePlayBack());

    return true;
}

/* static */ bool AppModuleGallery::rewind(void *userPtr, const char *optArg)
{
    AppModuleGallery *module = reinterpret_cast<AppModuleGallery*>(userPtr);

    PROPAGATE_ERROR(module->m_gallery.rewind());

    return true;
}

AppModuleGallery::AppModuleGallery()
    : m_initialized(false)
    , m_running(false)
    , m_guiContainerConfig(NULL)
    , m_guiConfig(NULL)
{
}

AppModuleGallery::~AppModuleGallery()
{
    shutdown();
}

bool AppModuleGallery::initialize(Options &options)
{
    if (m_initialized)
        return true;

    PROPAGATE_ERROR(m_gallery.initialize());

    m_initialized = true;

    return true;
}

bool AppModuleGallery::shutdown()
{
    if (!m_initialized)
        return true;

    PROPAGATE_ERROR_CONTINUE(stop());

    PROPAGATE_ERROR_CONTINUE(m_gallery.shutdown());

    m_initialized = false;

    return true;
}

bool AppModuleGallery::start(Window::IGuiMenuBar *iGuiMenuBar,
    Window::IGuiContainer *iGuiContainerConfig)
{
    if (m_running)
        return true;

    // register key observer
    PROPAGATE_ERROR(Window::getInstance().registerObserver(this));

    // initialize the GUI
    if (iGuiContainerConfig && !m_guiConfig)
    {
        // initialize the GUI

        // create a grid container
        PROPAGATE_ERROR(Window::IGuiContainerGrid::create(&m_guiConfig));

        // create the elements
        UniquePointer<Window::IGuiElement> element;
        unsigned int column = 0;

        PROPAGATE_ERROR(Window::IGuiElement::createAction("Previous Item",
            AppModuleGallery::prevItem, this, Window::IGuiElement::FLAG_NONE,
            Window::IGuiElement::ICON_PREVIOUS, &element));
        PROPAGATE_ERROR(m_guiConfig->attach(element.get(), column++, 0));
        element.release();

        PROPAGATE_ERROR(Window::IGuiElement::createAction("Rewind",
            AppModuleGallery::rewind, this, Window::IGuiElement::FLAG_NONE,
            Window::IGuiElement::ICON_MEDIA_REWIND, &element));
        PROPAGATE_ERROR(m_guiConfig->attach(element.get(), column++, 0));
        element.release();

        PROPAGATE_ERROR(Window::IGuiElement::createAction("Toggle Playback",
            AppModuleGallery::togglePlayBack, this, Window::IGuiElement::FLAG_BUTTON_TOGGLE,
            Window::IGuiElement::ICON_MEDIA_PLAY, &element));
        PROPAGATE_ERROR(m_guiConfig->attach(element.get(), column++, 0));
        element.release();

        PROPAGATE_ERROR(Window::IGuiElement::createAction("Next Item",
            AppModuleGallery::nextItem, this, Window::IGuiElement::FLAG_NONE,
            Window::IGuiElement::ICON_NEXT, &element));
        PROPAGATE_ERROR(m_guiConfig->attach(element.get(), column++, 0));
        element.release();

        m_guiContainerConfig = iGuiContainerConfig;
    }

    if (m_guiContainerConfig)
        PROPAGATE_ERROR(m_guiContainerConfig->add(m_guiConfig));

    PROPAGATE_ERROR(m_gallery.start());

    m_running = true;

    return true;
}

bool AppModuleGallery::stop()
{
    if (!m_running)
        return true;

    PROPAGATE_ERROR(m_gallery.stop());

    if (m_guiContainerConfig)
        PROPAGATE_ERROR(m_guiContainerConfig->remove(m_guiConfig));

    // unregister key observer
    PROPAGATE_ERROR(Window::getInstance().unregisterObserver(this));

    m_running = false;

    return true;
}

bool AppModuleGallery::onKey(const Key &key)
{
    if (key == Key("Left"))
    {
        PROPAGATE_ERROR(m_gallery.prevItem());
    }
    else if (key == Key("Right"))
    {
        PROPAGATE_ERROR(m_gallery.nextItem());
    }
    else if (key == Key("space"))
    {
        PROPAGATE_ERROR(m_gallery.togglePlayBack());
    }

    return true;
}

}; // namespace ArgusSamples
