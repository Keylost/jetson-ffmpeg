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

#ifndef WINDOW_BASE_H
#define WINDOW_BASE_H

#include <Argus/Argus.h>
#include <EGL/egl.h>

#include <list>
#include <string>

#include <string.h>

#include "UniquePointer.h"

/// Implementations of the WindowBase class set WINDOW_GUI_SUPPORT to one of these defines
#define WINDOW_GUI_NONE 0
#define WINDOW_GUI_GTK 1
#define WINDOW_GUI_GLX 2
#define WINDOW_GUI_ANDROID 3

namespace ArgusSamples
{

template<typename T> class Value;

/**
 * Base window class.
 */
class WindowBase
{
public:
    /**
     * Shutdown, free all resources
     */
    virtual bool shutdown() = 0;

    /**
     * Sleep for the requested number of seconds.
     * Once per second, this method will also poll for events.
     * @returns false if any of the pollEvents() calls fail.
     */
    bool pollingSleep(uint32_t seconds);

    /**
     * Interface for classes to get notified on pressed keys
     */
    class IKeyObserver
    {
    public:
        /**
         * Key modifier, indicates active modifiers, e.g. Shift or Control.
         */
        struct KeyModifier
        {
        public:
            KeyModifier(unsigned int m = 0)
                : mask(m)
            {
            }

            /**
             * Compare key modifiers
             */
            bool operator == (const KeyModifier& rhs) const
            {
                return (mask == rhs.mask);
            }

            unsigned int mask;                          ///! All modifiers

            static const unsigned int MASK_SHIFT = 1 << 0;     ///! Shift
            static const unsigned int MASK_LOCK = 1 << 1;      ///! Shift-Lock
            static const unsigned int MASK_CONTROL = 1 << 2;   ///! Control
            static const unsigned int MASK_MOD1 = 1 << 3;      ///! Modifier1
            static const unsigned int MASK_MOD2 = 1 << 4;      ///! Modifier2
            static const unsigned int MASK_MOD3 = 1 << 5;      ///! Modifier3
            static const unsigned int MASK_MOD4 = 1 << 6;      ///! Modifier4
            static const unsigned int MASK_MOD5 = 1 << 7;      ///! Modifier5
        };

        /**
         * Key, includes a key modifier and a key sym string which is a standard KeySym name
         * obtained from <X11/keysymdef.h> by removing the XK_ prefix from each name.
         */
        struct Key
        {
        public:
            explicit Key(const char *k, KeyModifier m = KeyModifier())
                : keySymString(k)
                , modifier(m)
            {
            }

            /**
             * Compare keys.
             */
            bool operator == (const Key& rhs) const
            {
                return
                    ((strcmp(keySymString, rhs.keySymString) == 0) &&
                     (modifier == rhs.modifier));
            }

            const char *keySymString;
            KeyModifier modifier;
        };

        /**
         * Called when a key is pressed
         *
         * @param [in] key
         */
        virtual bool onKey(const Key &key) = 0;

    protected:
        ~IKeyObserver() { }
    };

    /**
     * Register an observer notified when a key is pressed
     */
    bool registerObserver(IKeyObserver *observer);

    /**
     * Unregister an key observer
     */
    bool unregisterObserver(IKeyObserver *observer);

    /**
     * Interface for classes to get notified on window resize events
     */
    class IResizeObserver
    {
    public:
        /**
         * Called when the window is resized
         *
         * @param [in] width    new window width
         * @param [in] height    new window height
         */
        virtual bool onResize(uint32_t width, uint32_t height) = 0;

    protected:
        ~IResizeObserver() { }
    };

    /**
     * Add a call back function notified when the window had been resized
     */
    bool registerObserver(IResizeObserver *observer);

    /**
     * Unregister an resize observer
     */
    bool unregisterObserver(IResizeObserver *observer);

    /**
     * Poll events. Returns if all events are handled.
     */
    virtual bool pollEvents() = 0;

    /**
     * The event loop.
     */
    virtual bool eventLoop() = 0;

    /**
     * Request that the window is closed and the event loop is terminated. Can be called from
     * key callback.
     */
    virtual bool requestExit() = 0;

    /**
     * Gets the EGL native for the display.
     */
    virtual EGLNativeDisplayType getEGLNativeDisplay() const = 0;

    /**
     * Gets the EGL native for the window.
     */
    virtual EGLNativeWindowType getEGLNativeWindow() const = 0;

    /**
     * Gets the EGL surface for a given EGL config.
     */
    EGLSurface getEGLSurface(EGLConfig config);

    /**
     * Called on display termination to free allocated egl surfaces.
     */
    void onDisplayTermination(EGLDisplay display);

    /**
     * Get the width of the window
     */
    virtual uint32_t getWidth() const = 0;

    /**
     * Get the height of the window
     */
    virtual uint32_t getHeight() const  = 0;

    /**
     * Set the position and size of the window.
     */
    virtual bool setWindowRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) = 0;
    virtual bool setWindowRect(const Argus::Rectangle<uint32_t>& rect);

    /**
     * GUI element base class, all GUI elements and menus are derived from this.
     */
    class IGuiElement
    {
    public:
        virtual ~IGuiElement() {}

        /**
         * Flags which can be passed to some creation functions
         */
        enum Flags
        {
            FLAG_NONE = (0 << 0),
            FLAG_BUTTON_TOGGLE = (1 << 0),  ///< toggle button
            FLAG_NUMERIC_SLIDER = (1 << 1), ///< a numeric input has a slider
        };

        /**
         * Predefined icons
         */
        enum Icon
        {
            ICON_NONE,          ///< no icon, display text
            ICON_PREVIOUS,      ///< previous '<'
            ICON_NEXT,          ///< next '>'
            ICON_MEDIA_REWIND,  ///< rewind '|<-'
            ICON_MEDIA_PLAY,    ///< play '>'
            ICON_MEDIA_RECORD,  ///< record 'O'
        };

        /**
         * A common type for all enums. Use this type to create a GUI element for an enum.
         */
        enum ValueTypeEnum {};

        /**
         * Create a GUI element for a value. Implementations derived from WindowBase need to implement
         * specializations of this function
         */
        template<typename T> static bool createValue(Value<T> *value,
            IGuiElement **element, Flags flags = FLAG_NONE);

        /**
         * Create a GUI element for a label.
         */
        static bool createLabel(const char *labelText, IGuiElement **element);

        /**
         * Create a GUI element for a file chooser.
         *
         * @param value [in]        the value to update
         * @param pathsOnly [in]    if set then only select paths
         * @param element [out]     the created GUI element
         */
        static bool createFileChooser(Value<std::string> *value, bool pathsOnly,
            IGuiElement **element);

        /**
         * Action call back function.
         *
         * @param usrPtr [in]   user pointer
         * @param optArg [in]   optional argument string, NULL when there is no argument
         */
        typedef bool (*GuiActionCallBackFunc)(void *userPtr, const char *optArg);

        /**
         * Create a GUI element (a button), executing an action when activated.
         *
         * @param name [in]     text on the buttom
         * @param function [in] callback function
         * @param userPtr [in]  pointer passed to callback function
         * @param flags [in]    flags
         * @param icon [in]     predefined icon
         * @param element [out] the created GUI element
         */
        static bool createAction(const char *name, GuiActionCallBackFunc function, void *userPtr,
            Flags flags, Icon icon, IGuiElement **element);
    };

    /**
     * A GUI element displaying an image
     */
    class IGuiImage : public IGuiElement
    {
    public:
        virtual ~IGuiImage() {}

        /**
         * Create a GUI element for an image
         *
         * @param element [out]     the created GUI element
         */
        static bool create(IGuiImage **image);

        /**
         * Set the image data.
         *
         * @param width [in]    width of the image
         * @param height [in]   height of the image
         * @param components [in] color components per pixel (only 3 or 4 is supported)
         * @param rowStride [in] bytes between consecutive rows
         * @param data [in]     image data
         */
        virtual bool set(size_t width, size_t height, uint32_t components, size_t rowStride,
            const uint8_t *data) = 0;
    };

    /**
     * A GUI menu item
     */
    class IGuiMenuItem : public IGuiElement
    {
    public:
        virtual ~IGuiMenuItem() {}

        /**
         * Call back function.
         *
         * @param [in] usrPtr   user pointer
         * @param [in] optArg   optional argument string, NULL when there is no argument
         */
        typedef bool (*CallBackFunc)(void *userPtr, const char *optArg);

        /**
         * Create a GUI menu item, the call back is executed when the item is selected.
         */
        static bool create(const char *name, CallBackFunc function, void *userPtr,
            IGuiMenuItem **iGuiMenuItem);
    };

    /**
     * A GUI menu (e.g. File, Edit, ...), it contains IGuiMenuItem items.
     */
    class IGuiMenu : public IGuiElement
    {
    public:
        virtual ~IGuiMenu() {}

        /**
         * Create a menu.
         */
        static bool create(const char *name, IGuiMenu **iGuiMenu);

        /**
         * Add a menu item.
         */
        virtual bool add(IGuiMenuItem *menuItem) = 0;

        /**
         * Remove a menu item.
         */
        virtual bool remove(IGuiMenuItem *menuItem) = 0;
    };

    /**
     * A GUI menu bar, the menu bar holds IGuiMenu items (e.g. File, Edit, ...)
     */
    class IGuiMenuBar : public IGuiElement
    {
    public:
        virtual ~IGuiMenuBar() {}

        /**
         * Create a menu bar.
         */
        static bool create(IGuiMenuBar **iGuiMenuBar);

        /**
         * Add a menu.
         */
        virtual bool add(IGuiMenu *menu) = 0;

        /**
         * Remove a menu.
         */
        virtual bool remove(IGuiMenu *menu) = 0;

        /**
         * Get a IGuiMenu item named 'name'
         */
        virtual IGuiMenu *getMenu(const char *name) = 0;
    };

    /**
     * A GUI container
     */
    class IGuiContainer : public IGuiElement
    {
    public:
        virtual ~IGuiContainer() {}

        /**
         * Add a GUI element to the container. The container takes the ownership of the element.
         *
         * @param [in] iGuiElement  GUI element to add
         */
        virtual bool add(IGuiElement *iGuiElement) = 0;

        /**
         * Remove a GUI element from the container. The element is not deleted but ownership is
         * passed back to the caller.
         *
         * @param [in] iGuiElement  GUI element to remove
         */
        virtual bool remove(IGuiElement *iGuiElement) = 0;
    };

    /**
     * A GUI container organized as a grid
     */
    class IGuiContainerGrid : public IGuiContainer
    {
    public:
        virtual ~IGuiContainerGrid() {}

        /**
         * Create a GUI element containing multiple GUI elements organized in a grid. The GUI
         * container takes the ownership of the elements added to it.
         *
         * @param [out] iGuiContainer   the created GUI container
         */
        static bool create(IGuiContainerGrid **iGuiContainer);

        /**
         * Attach a GUI element to the container at a given position with a specified size. The
         * container takes the ownership of the element.
         *
         * @param [in] iGuiElement  GUI element to add
         * @param [in] left, top    position of the element
         * @param [in] width, height    size of the element
         */
        virtual bool attach(IGuiElement *iGuiElement, unsigned int left, unsigned int top,
            unsigned int width = 1, unsigned int height = 1) = 0;

        /**
         * Simplifies adding elements to a grid
         */
        class BuildHelper
        {
        public:
            BuildHelper(IGuiContainerGrid *container);

            /**
             * Append an element, the element can span multiple rows and/or columns
             *
             * @param [in] iGuiElement  GUI element to append
             * @param [in] width, height    size of the element
             */
            bool append(IGuiElement *iGuiElement, uint32_t width = 1, uint32_t height = 1);

            /**
             * Append an element with a label
             */
            bool append(const char *label, IGuiElement *iGuiElement);

        private:
            IGuiContainerGrid *m_container;
            uint32_t m_row;
        };
    };

    /**
     * Set the window GUI. 'iGuiContainer' contains 'iGuiElementWindow' and 'iGuiElementView'.
     * If 'iGuiContainer' is NULL the default GUI is used.
     * When the function succeeds the class takes the ownership of the passed in elements.
     *
     * @param iGuiContainer [in]        GUI container
     * @param iGuiElementWindow [in]    window GUI element
     * @param iGuiElementView [in]      view GUI element, the EGL native for the window/display
     *                                  will be taken from this
     */
    virtual bool setWindowGui(IGuiContainer *iGuiContainer = NULL,
        IGuiElement *iGuiElementWindow = NULL, IGuiElement *iGuiElementView = NULL)
    {
        return true;
    }

protected:
    WindowBase();
    virtual ~WindowBase();

    virtual bool initialize() = 0;

    /**
     * Key press event
     *
     * @param key [in]
     */
    bool eventKeyPress(const IKeyObserver::Key &key);

    /**
     * Resize event
     */
    bool eventResize(uint32_t width, uint32_t height);

    class Surface
    {
    public:
        Surface();
        ~Surface();

        bool create(WindowBase *window, EGLConfig config);

        EGLDisplay m_display;
        EGLConfig m_config;
        EGLSurface m_eglSurface;
    };

    typedef std::list<Surface*> SurfaceList;
    SurfaceList m_surfaces;

    typedef std::list<IKeyObserver*> IKeyObserverList;
    IKeyObserverList m_keyObservers;
    typedef std::list<IResizeObserver*> IResizeObserverList;
    IResizeObserverList m_resizeObservers;

private:
    // this is a singleton, hide copy constructor etc.
    WindowBase(const WindowBase&);
    WindowBase& operator=(const WindowBase&);
};


}; // namespace ArgusSamples

#endif // WINDOW_BASE_H
