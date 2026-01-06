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

#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <assert.h>
#include <fcntl.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>   // for gdk_x11_*

#include "GuiElement.h"

#include "Error.h"
#include "InitOnce.h"
#include "UniquePointer.h"

// the X Window collides with our Window class
typedef ::Window XWindow;

namespace ArgusSamples
{

static const uint32_t DEFAULT_LEFT = 0;
static const uint32_t DEFAULT_TOP = 0;
static const uint32_t DEFAULT_WIDTH = 1024;
static const uint32_t DEFAULT_HEIGHT = 768;

/**
 * Private data for GTK window to avoid including GTK headers in Window.h
 */
class Window::PrivateData
{
public:
    PrivateData()
        : m_initialized(false)
        , m_guiContainerWindow(NULL)
        , m_guiElementWindow(NULL)
        , m_guiElementView(NULL)
        , m_window(NULL)
        , m_view(NULL)
        , m_left(DEFAULT_LEFT)
        , m_top(DEFAULT_TOP)
        , m_width(DEFAULT_WIDTH)
        , m_height(DEFAULT_HEIGHT)
        , m_gIoChannel(NULL)
    {
        m_signalPipe[0] = 0;
        m_signalPipe[1] = 0;
    }
    ~PrivateData()
    {
        delete m_guiContainerWindow;
        delete m_guiElementWindow;
        delete m_guiElementView;
    }

    bool m_initialized;     ///< if set the window is initialized

    IGuiContainer *m_guiContainerWindow;    ///< window GUI container
    IGuiElement *m_guiElementWindow;        ///< window element
    IGuiElement *m_guiElementView;          ///< view element

    GtkWindow *m_window;    ///< window
    GtkWidget *m_view;      ///< view

    uint32_t m_left;        ///< window x offset
    uint32_t m_top;         ///< window y offset
    uint32_t m_width;       ///< window width
    uint32_t m_height;      ///< window height

    int m_signalPipe[2];    ///< caught Unix signals are written to this pipe
    GIOChannel *m_gIoChannel;   ///< IO channel delivering signals from the pipe to GTK
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

class Window::SignalCallbacks
{
public:
    static void signalHandler(int signo);
    static gboolean signalWatch(GIOChannel *source, GIOCondition cond, gpointer data);
};

/**
 * The Unix signal handler. Writes the signal to the signal pipe.
 */
/*static*/ void Window::SignalCallbacks::signalHandler(int signo)
{
    if (write(Window::getInstance().m_private->m_signalPipe[1], &signo, sizeof(int)) != sizeof(int))
    {
        REPORT_ERROR("Unix signal %d lost\n", signo);
    }
}

/**
 * The event loop callback that handles the unix signals delivered by the signal pipe.
 */
/*static*/ gboolean Window::SignalCallbacks::signalWatch(GIOChannel *source, GIOCondition cond,
    gpointer data)
{
    Window *window = static_cast<Window*>(data);
    bool done = false;

    while (!done)
    {
        int signum = 0;
        gsize bytesRead = 0;
        const GIOStatus status = g_io_channel_read_chars(source, reinterpret_cast<gchar*>(&signum),
            sizeof(signum), &bytesRead, NULL);
        switch (status)
        {
        case G_IO_STATUS_NORMAL:
            if (bytesRead != sizeof(signum))
                ORIGINATE_ERROR("Expected %zu bytes", sizeof(signum));
            if ((signum == SIGINT) || (signum == SIGTSTP))
            {
                PROPAGATE_ERROR(window->requestExit());
                done = true;
            }
            else
                ORIGINATE_ERROR("Unexpected signal");
            break;
        case G_IO_STATUS_ERROR:
            ORIGINATE_ERROR("Error reading from channel");
            break;
        case G_IO_STATUS_EOF:
            ORIGINATE_ERROR("Channel had been closed");
            break;
        case G_IO_STATUS_AGAIN:
            done = true;
            break;
        }
    }

    return true;
}

bool Window::setWindowRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    assert(m_private);

    if (m_private->m_window)
    {
        gtk_window_move(m_private->m_window, y, y);
        gtk_window_resize(m_private->m_window, w, h);
    }

    m_private->m_left = x;
    m_private->m_top = y;
    m_private->m_width = w;
    m_private->m_height = h;

    return true;
}

static void logHandler(const gchar *logDomain, GLogLevelFlags logLevel, const gchar *message,
    gpointer user_data)
{
    const char *level;
    switch (logLevel)
    {
    case G_LOG_LEVEL_ERROR:
        level = "ERROR";
        break;
    case G_LOG_LEVEL_CRITICAL:
        level = "CRITICAL";
        break;
    case G_LOG_LEVEL_WARNING:
        level = "WARNING";
        break;
    case G_LOG_LEVEL_MESSAGE:
        level = "MESSAGE";
        break;
    case G_LOG_LEVEL_INFO:
        level = "INFO";
        break;
    case G_LOG_LEVEL_DEBUG:
        level = "DEBUG";
        break;
    default:
        level = "UNKNOWN";
        break;
    }

    REPORT_ERROR("%s: %s, %s", logDomain, level, message);
}

bool Window::initialize()
{
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

    gtk_init(NULL, NULL);

    g_log_set_handler("Gtk", static_cast<GLogLevelFlags>(G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL),
        logHandler, NULL);

    // Catch Unix signals. The signal callback can't directly call into GTK, therefore write
    // to a pipe and install a handler to read from the pipe.

    // create the pipe
    if (pipe(m_private->m_signalPipe) != 0)
        ORIGINATE_ERROR("Failed to create the signal pipe");

    // put the pipe into non-blocking mode.
    const long fdFlags = fcntl(m_private->m_signalPipe[1], F_GETFL);
    if (fdFlags == -1)
        ORIGINATE_ERROR("Failed to read descriptor flags");
    if (fcntl(m_private->m_signalPipe[1], F_SETFL, fdFlags | O_NONBLOCK) == -1)
        ORIGINATE_ERROR("Failed to write descriptor flags");

    // install the unix signal handler for the signals of interest
    if (signal(SIGINT, SignalCallbacks::signalHandler) == SIG_ERR)
        ORIGINATE_ERROR("Cannot register handler for SIGINT");

    if (signal(SIGTSTP, SignalCallbacks::signalHandler) == SIG_ERR)
        ORIGINATE_ERROR("Cannot register handler for SIGTSTP");

    // convert the reading end of the pipe into a GIOChannel
    m_private->m_gIoChannel = g_io_channel_unix_new(m_private->m_signalPipe[0]);
    if (!m_private->m_gIoChannel)
        ORIGINATE_ERROR("g_io_channel_unix_new failed");

    // we read raw binary data without interpretation
    if (g_io_channel_set_encoding(m_private->m_gIoChannel, NULL, NULL) != G_IO_STATUS_NORMAL)
        ORIGINATE_ERROR("g_io_channel_set_encoding failed");

    // add the IO channel to the event loop
    g_io_add_watch(m_private->m_gIoChannel,
        static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP), SignalCallbacks::signalWatch,
        this);

    m_private->m_initialized = true;

    return true;
}

/**
 * Shutdown the Window
 */
bool Window::shutdown()
{
    if (!m_private)
        return true;

    if (m_private->m_gIoChannel)
    {
        if (g_io_channel_shutdown(m_private->m_gIoChannel, TRUE, NULL) != G_IO_STATUS_NORMAL)
            REPORT_ERROR("g_io_channel_shutdown failed");
        m_private->m_gIoChannel = NULL;
    }

    if (m_private->m_signalPipe[0])
    {
        close(m_private->m_signalPipe[0]);
        m_private->m_signalPipe[0] = 0;
    }

    if (m_private->m_signalPipe[1])
    {
        close(m_private->m_signalPipe[1]);
        m_private->m_signalPipe[1] = 0;
    }

    // unregister signal handlers
    if (signal(SIGINT, SIG_DFL) == SIG_ERR)
        REPORT_ERROR("Cannot unregister handler for SIGINT");

    if (signal(SIGTSTP, SIG_DFL) == SIG_ERR)
        REPORT_ERROR("Cannot unregister handler for SIGTSTP");

    delete m_private;
    m_private = NULL;

    return true;
}

class Window::gtkCallBacks
{
public:
    static gboolean onClose(GtkWidget *widget, GdkEvent *event, gpointer userData);
    static gboolean onKeyPress(GtkWidget *widget, GdkEvent *event, gpointer userData);
    static gboolean onConfigure(GtkWidget *widget, GdkEvent *event, gpointer userData);
    static gboolean onButtonPress(GtkWidget *widget, GdkEvent *event, gpointer userData);
};

/* static */ gboolean Window::gtkCallBacks::onClose(GtkWidget *widget, GdkEvent *event,
    gpointer userData)
{
    Window *window = static_cast<Window*>(userData);
    PROPAGATE_ERROR(window->requestExit());
    return TRUE;
}

/* static */ gboolean Window::gtkCallBacks::onKeyPress(GtkWidget *widget, GdkEvent *event,
    gpointer userData)
{
    Window *window = static_cast<Window*>(userData);

    unsigned int mask = 0;

    if (event->key.state & ShiftMask)
        mask |= GDK_SHIFT_MASK;
    if (event->key.state & LockMask)
        mask |= GDK_LOCK_MASK;
    if (event->key.state & ControlMask)
        mask |= GDK_CONTROL_MASK;
    if (event->key.state & Mod1Mask)
        mask |= GDK_MOD1_MASK;
    if (event->key.state & Mod2Mask)
        mask |= GDK_MOD2_MASK;
    if (event->key.state & Mod3Mask)
        mask |= GDK_MOD3_MASK;
    if (event->key.state & Mod4Mask)
        mask |= GDK_MOD4_MASK;
    if (event->key.state & Mod5Mask)
        mask |= GDK_MOD5_MASK;

    const char *keyName = gdk_keyval_name(event->key.keyval);
    if (!keyName)
        ORIGINATE_ERROR("gdk_keyval_name failed");

    PROPAGATE_ERROR(window->eventKeyPress(IKeyObserver::Key(keyName, mask)));

    // return FALSE to propagate the event further
    return FALSE;
}

/* static */ gboolean Window::gtkCallBacks::onConfigure(GtkWidget *widget, GdkEvent *event,
    gpointer userData)
{
    Window *window = static_cast<Window*>(userData);
    Window::PrivateData *privateData = window->m_private;

    // call resize only if our window-size changed
    if ((static_cast<uint32_t>(event->configure.width) != privateData->m_width) ||
        (static_cast<uint32_t>(event->configure.height) != privateData->m_height))
    {
        privateData->m_width = event->configure.width;
        privateData->m_height = event->configure.height;
        PROPAGATE_ERROR(window->eventResize(privateData->m_width, privateData->m_height));
    }

    return TRUE;
}

/* static */ gboolean Window::gtkCallBacks::onButtonPress(GtkWidget *widget, GdkEvent *event,
    gpointer userData)
{
    Window *window = static_cast<Window*>(userData);
    Window::PrivateData *privateData = window->m_private;

    // if clicking on the view grab focus
    if (widget == privateData->m_view)
    {
        gtk_widget_grab_focus(widget);
    }

    return TRUE;
}

bool Window::pollEvents()
{
    // create the default GUI if none had provided
    PROPAGATE_ERROR(setWindowGui());

    while (gtk_events_pending())
        gtk_main_iteration();

    return true;
}

bool Window::eventLoop()
{
    // create the default GUI if none had provided
    PROPAGATE_ERROR(setWindowGui());

    gtk_main();

    return true;
}

bool Window::requestExit()
{
    gtk_main_quit();
    return true;
}

EGLNativeDisplayType Window::getEGLNativeDisplay() const
{
    Display *xdisplay = NULL;

    // create the default GUI if none had provided
    PROPAGATE_ERROR_CONTINUE(const_cast<Window*>(this)->setWindowGui());

    if (m_private)
    {
        GdkScreen *screen = gtk_window_get_screen(m_private->m_window);
        if (screen)
        {
            GdkDisplay *display = gdk_screen_get_display(screen);
            if (display)
                xdisplay = gdk_x11_display_get_xdisplay(display);
        }
    }

    return static_cast<EGLNativeDisplayType>(xdisplay);
}

EGLNativeWindowType Window::getEGLNativeWindow() const
{
    XWindow xwindow = None;

    // create the default GUI if none had provided
    PROPAGATE_ERROR_CONTINUE(const_cast<Window*>(this)->setWindowGui());

    if (m_private)
    {
        GdkWindow *window = gtk_widget_get_window(m_private->m_view);
        if (window)
            xwindow = gdk_x11_window_get_xid(window);
    }

#if defined(WIN_INTERFACE_CUSTOM)
    return reinterpret_cast<EGLNativeWindowType>(xwindow);
#else
    return static_cast<EGLNativeWindowType>(xwindow);
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

bool Window::setWindowGui(IGuiContainer *iGuiContainer, IGuiElement *iGuiElementWindow,
    IGuiElement *iGuiElementView)
{
    if ((iGuiContainer != NULL) && ((iGuiElementWindow == NULL) || (iGuiElementView == NULL)))
        ORIGINATE_ERROR("Window and view element need to be specified");

    PROPAGATE_ERROR(initialize());

    if (m_private->m_guiContainerWindow)
        return true;

    // use these to make sure all created elements are freed on error
    UniquePointer<IGuiContainer> createdContainer;
    UniquePointer<IGuiElement> createdWindow;
    UniquePointer<IGuiElement> createdView;

    if (iGuiContainer == NULL)
    {
        // use the default window if no builder string is given
        static const char defaultWindow[] =
        {
            "<?xml version='1.0' encoding='UTF-8'?>"
            "<interface>"
            "  <!-- interface-requires gtk+ 3.0 -->"
            "  <object class='GtkWindow' id='window'>"
            "    <property name='can_focus'>False</property>"
            "    <child>"
            "      <object class='GtkDrawingArea' id='view'>"
            "        <property name='visible'>True</property>"
            "        <property name='can_focus'>False</property>"
            "      </object>"
            "    </child>"
            "  </object>"
            "</interface>"
        };

        IGuiBuilder *iGuiBuilder = NULL;
        PROPAGATE_ERROR(IGuiBuilder::create(defaultWindow, &iGuiBuilder));
        createdContainer.reset(iGuiBuilder);

        iGuiElementWindow = iGuiBuilder->createElement("window");
        if (!iGuiElementWindow)
            ORIGINATE_ERROR("Element 'window' not found");
        createdWindow.reset(iGuiElementWindow);

        iGuiElementView = iGuiBuilder->createElement("view");
        if (!iGuiElementView)
            ORIGINATE_ERROR("Element 'view' not found");
        createdView.reset(iGuiElementView);
    }

    m_private->m_window = GTK_WINDOW(static_cast<GuiElement*>(iGuiElementWindow)->getWidget());
    m_private->m_view = static_cast<GuiElement*>(iGuiElementView)->getWidget();

    //gtk_widget_set_double_buffered(m_private->m_view, FALSE);

    // enable and connect events
    if (g_signal_connect(m_private->m_window, "delete-event",
        G_CALLBACK(gtkCallBacks::onClose), this) == 0)
    {
        ORIGINATE_ERROR("Failed to connect signal");
    }

    // the view is used for size and key events
    gtk_widget_add_events(m_private->m_view, GDK_KEY_PRESS_MASK);
    // make sure the view can have the keyboard focus, get focus when clicked on it with the mouse
    // and currently has the focus
    gtk_widget_set_can_focus(m_private->m_view, TRUE);
    gtk_widget_grab_focus(m_private->m_view);
    if (g_signal_connect(m_private->m_view, "key-press-event",
        G_CALLBACK(gtkCallBacks::onKeyPress), this) == 0)
    {
       ORIGINATE_ERROR("Failed to connect signal");
    }
    gtk_widget_add_events(m_private->m_view, GDK_STRUCTURE_MASK);
    if (g_signal_connect(m_private->m_view, "configure-event",
        G_CALLBACK(gtkCallBacks::onConfigure), this) == 0)
    {
       ORIGINATE_ERROR("Failed to connect signal");
    }
    gtk_widget_add_events(m_private->m_view, GDK_BUTTON_PRESS_MASK);
    if (g_signal_connect(m_private->m_view, "button-press-event",
        G_CALLBACK(gtkCallBacks::onButtonPress), this) == 0)
    {
       ORIGINATE_ERROR("Failed to connect signal");
    }

    gtk_window_move(m_private->m_window, m_private->m_left, m_private->m_top);
    gtk_window_resize(m_private->m_window, m_private->m_width, m_private->m_height);

    // show the window
    gtk_widget_show_now(GTK_WIDGET(m_private->m_window));

    // take ownership
    if (createdContainer)
        m_private->m_guiContainerWindow = createdContainer.release();
    else
        m_private->m_guiContainerWindow = iGuiContainer;
    if (createdWindow)
        m_private->m_guiElementWindow = createdWindow.release();
    else
        m_private->m_guiElementWindow = iGuiElementWindow;
    if (createdView)
        m_private->m_guiElementView = createdView.release();
    else
        m_private->m_guiElementView = iGuiElementView;

    return true;
}

}; // namespace ArgusSamples
