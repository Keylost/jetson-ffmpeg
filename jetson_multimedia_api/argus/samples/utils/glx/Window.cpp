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

#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <signal.h>
#include <assert.h>

#include "Window.h"

#include "Error.h"
#include "InitOnce.h"

// the X Window collides with our Window class
typedef ::Window XWindow;

namespace ArgusSamples
{

static const uint32_t DEFAULT_LEFT = 0;
static const uint32_t DEFAULT_TOP = 0;
static const uint32_t DEFAULT_WIDTH = 1024;
static const uint32_t DEFAULT_HEIGHT = 768;

/**
 * Private data for GLX window to avoid including X11 headers in Window.h
 */
class Window::PrivateData
{
public:
    PrivateData()
        : m_initialized(false)
        , m_requestExit(false)
        , m_display(NULL)
        , m_window(None)
        , m_left(DEFAULT_LEFT)
        , m_top(DEFAULT_TOP)
        , m_width(DEFAULT_WIDTH)
        , m_height(DEFAULT_HEIGHT)
    {
    }

    bool m_initialized;     ///< if set the window is initialized
    bool m_requestExit;     ///< if set request exit from the main loop

    Display *m_display;     ///< display
    XWindow m_window;       ///< window

    uint32_t m_left;        ///< window x offset
    uint32_t m_top;         ///< window y offset
    uint32_t m_width;       ///< window width
    uint32_t m_height;      ///< window height
};

Window::Window()
    : m_private(NULL)
{
}

Window::~Window()
{
    if (!shutdown())
        REPORT_ERROR("Failed to shutdown");
}

Window &Window::getInstance()
{
    static InitOnce initOnce;
    static Window instance;

    if (initOnce.begin())
    {
        if (instance.initialize())
        {
            initOnce.complete();
        }
        else
        {
            initOnce.failed();
            REPORT_ERROR("Initalization failed");
        }
    }

    return instance;
}

static void signalCallback(int signo)
{
    Window::getInstance().requestExit();
}

bool Window::setWindowRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    assert(m_private);

    XMoveResizeWindow(m_private->m_display, m_private->m_window, x, y, w, h);

    m_private->m_left = x;
    m_private->m_top = y;
    m_private->m_width = w;
    m_private->m_height = h;

    return true;
}

bool Window::initialize()
{
    // WAR, some of the kernel vi/isp drivers are not cleaning up
    // on non clean app exit, this catches sys interrupts and lets
    // the pipeline close explicitly.
    if (signal(SIGINT, signalCallback) == SIG_ERR)
    {
        PROPAGATE_ERROR("Cannot register handler for SIGINT");
        return false;
    }

    if (signal(SIGTSTP, signalCallback) == SIG_ERR)
    {
        PROPAGATE_ERROR("Cannot register handler for SIGTSTP");
        return false;
    }

    if (!m_private)
    {
        m_private = new PrivateData();
        if (!m_private)
            ORIGINATE_ERROR("Out of memory");
    }

    if (m_private->m_initialized)
        return true;

    if (XInitThreads() == 0)
        ORIGINATE_ERROR("XInitThreads() failed");

    m_private->m_display = XOpenDisplay(NULL);
    if (!m_private->m_display)
        ORIGINATE_ERROR("Failed to open X display (WARNING! Is DISPLAY environment variable set?)");

    XSetWindowAttributes swa;
    swa.event_mask = KeyPressMask | StructureNotifyMask;

    XWindow root = DefaultRootWindow(m_private->m_display);
    if (root == None)
        ORIGINATE_ERROR("Failed to get default root window");
    m_private->m_window = XCreateWindow(m_private->m_display, root,
        m_private->m_left, m_private->m_top, m_private->m_width, m_private->m_height,   0,
        CopyFromParent, CopyFromParent,
        CopyFromParent, CWEventMask,
        &swa);
    if (m_private->m_window == None)
        ORIGINATE_ERROR("Failed to create window");

    Atom wm_delete = XInternAtom(m_private->m_display, "WM_DELETE_WINDOW", 1);
    if (wm_delete == None)
        ORIGINATE_ERROR("Failed to get atom");
    if (XSetWMProtocols(m_private->m_display, m_private->m_window, &wm_delete, 1) == 0)
        ORIGINATE_ERROR("Failed to set WM protocols");

    XMapWindow(m_private->m_display, m_private->m_window);

    m_private->m_initialized = true;

    return true;
}

/**
 * Shutdown the Window
 */
bool Window::shutdown()
{
    if (!m_private || !m_private->m_initialized)
        return true;

    if (m_private->m_display)
    {
        if (m_private->m_window != None)
        {
            XDestroyWindow(m_private->m_display, m_private->m_window);
            m_private->m_window = None;
        }
        XCloseDisplay(m_private->m_display);
        m_private->m_display = NULL;
    }

    delete m_private;
    m_private = NULL;

    return true;
}

bool Window::pollEvents()
{
    assert(m_private);

    while (XPending(m_private->m_display))
    {
        XEvent event;

        XNextEvent(m_private->m_display, &event);
        if (event.xany.window != m_private->m_window)
            continue;

        switch (event.type)
        {
        case ConfigureNotify:
            // call resize only if our window-size changed
            if ((static_cast<uint32_t>(event.xconfigure.width) != m_private->m_width) ||
                (static_cast<uint32_t>(event.xconfigure.height) != m_private->m_height))
            {
                m_private->m_width = event.xconfigure.width;
                m_private->m_height = event.xconfigure.height;
                PROPAGATE_ERROR(eventResize(m_private->m_width, m_private->m_height));
            }
            break;
        case KeyPress:
            {
                unsigned int mask = 0;

                if (event.xkey.state & ShiftMask)
                    mask |= IKeyObserver::KeyModifier::MASK_SHIFT;
                if (event.xkey.state & LockMask)
                    mask |= IKeyObserver::KeyModifier::MASK_LOCK;
                if (event.xkey.state & ControlMask)
                    mask |= IKeyObserver::KeyModifier::MASK_CONTROL;
                if (event.xkey.state & Mod1Mask)
                    mask |= IKeyObserver::KeyModifier::MASK_MOD1;
                if (event.xkey.state & Mod2Mask)
                    mask |= IKeyObserver::KeyModifier::MASK_MOD2;
                if (event.xkey.state & Mod3Mask)
                    mask |= IKeyObserver::KeyModifier::MASK_MOD3;
                if (event.xkey.state & Mod4Mask)
                    mask |= IKeyObserver::KeyModifier::MASK_MOD4;
                if (event.xkey.state & Mod5Mask)
                    mask |= IKeyObserver::KeyModifier::MASK_MOD5;

                int keysymsPerKeycode = 0;
                KeySym *keySym = XGetKeyboardMapping(m_private->m_display, event.xkey.keycode,
                    1, &keysymsPerKeycode);
                if (!keySym)
                    ORIGINATE_ERROR("XGetKeyboardMapping failed");

                const char *keySymString = XKeysymToString(keySym[0]);
                if (!keySymString)
                    ORIGINATE_ERROR("XKeysymToString failed");

                XFree(keySym);

                PROPAGATE_ERROR(eventKeyPress(IKeyObserver::Key(keySymString, mask)));
            }
            break;
        case ClientMessage:
            // depends on 3rdparty stubs change
            if (strcmp(XGetAtomName(m_private->m_display, event.xclient.message_type),
                "WM_PROTOCOLS") == 0)
            {
                PROPAGATE_ERROR(requestExit());
                return true;
            }
        default:
            break;
        }
    }

    return true;
}

bool Window::eventLoop()
{
    assert(m_private);

    while (!m_private->m_requestExit)
    {
        PROPAGATE_ERROR(pollEvents());
        usleep(33000);
    }

    return true;
}

bool Window::requestExit()
{
    assert(m_private);

    m_private->m_requestExit = true;
    return true;
}

EGLNativeDisplayType Window::getEGLNativeDisplay() const
{
    Display *display = NULL;

    if (m_private)
        display = m_private->m_display;

    return static_cast<EGLNativeDisplayType>(display);
}

EGLNativeWindowType Window::getEGLNativeWindow() const
{
    XWindow window = None;

    if (m_private)
        window = m_private->m_window;

#if defined(WIN_INTERFACE_CUSTOM)
    return reinterpret_cast<EGLNativeWindowType>(window);
#else
    return static_cast<EGLNativeWindowType>(window);
#endif
}

uint32_t Window::getWidth() const
{
    if (m_private)
        return m_private->m_width;

    return 0;
}

uint32_t Window::getHeight() const
{
    if (m_private)
        return m_private->m_height;

    return 0;
}

}; // namespace ArgusSamples
