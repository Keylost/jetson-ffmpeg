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

#include "WindowBase.h"

#include "Error.h"
#include "UniquePointer.h"

#include <unistd.h>

namespace ArgusSamples
{

WindowBase::WindowBase()
{
}

WindowBase::~WindowBase()
{
    for (SurfaceList::iterator it = m_surfaces.begin(); it != m_surfaces.end(); ++it)
    {
        delete *it;
    }
    m_surfaces.clear();
}

bool WindowBase::registerObserver(IKeyObserver *observer)
{
    m_keyObservers.push_back(observer);
    return true;
}

bool WindowBase::unregisterObserver(IKeyObserver *observer)
{
    m_keyObservers.remove(observer);
    return true;
}

bool WindowBase::registerObserver(IResizeObserver *observer)
{
    m_resizeObservers.push_back(observer);
    return true;
}

bool WindowBase::unregisterObserver(IResizeObserver *observer)
{
    m_resizeObservers.remove(observer);
    return true;
}

WindowBase::Surface::Surface()
    : m_display(EGL_NO_DISPLAY)
    , m_config(0)
    , m_eglSurface(EGL_NO_SURFACE)
{
}

WindowBase::Surface::~Surface()
{
    if (m_eglSurface != EGL_NO_SURFACE)
    {
        if (!eglDestroySurface(m_display, m_eglSurface))
            REPORT_ERROR("Failed to destroy EGL surface (error 0x%04x)", eglGetError());
        m_eglSurface = EGL_NO_SURFACE;
    }
}

bool WindowBase::Surface::create(WindowBase *window, EGLConfig config)
{
    if (m_eglSurface != EGL_NO_SURFACE)
        ORIGINATE_ERROR("Already created");

    m_display = eglGetDisplay(window->getEGLNativeDisplay());
    m_eglSurface = eglCreateWindowSurface(m_display, config, window->getEGLNativeWindow(), NULL);
    if (m_eglSurface == EGL_NO_SURFACE)
        ORIGINATE_ERROR("Could not create EGL surface (error 0x%04x)", eglGetError());

    m_config = config;

    return true;
}

bool WindowBase::setWindowRect(const Argus::Rectangle<uint32_t>& rect)
{
    return setWindowRect(rect.left(), rect.top(), rect.width(), rect.height());
}

bool WindowBase::pollingSleep(uint32_t seconds)
{
    for (uint32_t tick = 0; tick < seconds; ++tick)
    {
        PROPAGATE_ERROR(pollEvents());
        ::sleep(1);
    }

    // In case a sample calls this with zero seconds, perform a single poll.
    // This prevents lockups in a few of the samples.
    if (seconds == 0)
        PROPAGATE_ERROR(pollEvents());

    return true;
}

EGLSurface WindowBase::getEGLSurface(EGLConfig config)
{
    for (SurfaceList::iterator it = m_surfaces.begin(); it != m_surfaces.end(); ++it)
    {
        const Surface *surface = *it;
        if (surface->m_config == config)
            return surface->m_eglSurface;
    }

    UniquePointer<Surface> surface(new Surface());
    if (!surface)
    {
        REPORT_ERROR("Out of memory");
        return EGL_NO_SURFACE;
    }

    if (!surface->create(this, config))
        return EGL_NO_SURFACE;

    const EGLSurface eglSurface = surface->m_eglSurface;

    m_surfaces.push_back(surface.release());

    return eglSurface;
}

void WindowBase::onDisplayTermination(EGLDisplay display)
{
    // When a display is terminated free all surfaces for the display
    for (SurfaceList::iterator it = m_surfaces.begin(); it != m_surfaces.end();)
    {
        Surface *surface = *it;
        if (surface->m_display == display)
        {
            delete surface;
            it = m_surfaces.erase(it);
        }
        else
        {
            it++;
        }
    }

    return;
}

bool WindowBase::eventKeyPress(const IKeyObserver::Key &key)
{
    for (IKeyObserverList::iterator it = m_keyObservers.begin();
         it != m_keyObservers.end(); ++it)
    {
        PROPAGATE_ERROR((*it)->onKey(key));
    }

    return true;
}

bool WindowBase::eventResize(uint32_t width, uint32_t height)
{
    for (IResizeObserverList::iterator it = m_resizeObservers.begin();
         it != m_resizeObservers.end(); ++it)
    {
        PROPAGATE_ERROR((*it)->onResize(width, height));
    }

    return true;
}


WindowBase::IGuiContainerGrid::BuildHelper::BuildHelper(IGuiContainerGrid *container)
    : m_container(container)
    , m_row(0)
{
}

bool WindowBase::IGuiContainerGrid::BuildHelper::append(IGuiElement *iGuiElement, uint32_t width,
    uint32_t height)
{
    PROPAGATE_ERROR(m_container->attach(iGuiElement, 0, m_row, width, height));
    ++m_row;

    return true;
}

bool WindowBase::IGuiContainerGrid::BuildHelper::append(const char *label, IGuiElement *iGuiElement)
{
    UniquePointer<IGuiElement> labelElement;
    PROPAGATE_ERROR(IGuiElement::createLabel(label, &labelElement));
    PROPAGATE_ERROR(m_container->attach(labelElement.get(), 0, m_row));
    labelElement.release();

    PROPAGATE_ERROR(m_container->attach(iGuiElement, 1, m_row));
    ++m_row;

    return true;
}

}; // namespace ArgusSamples
