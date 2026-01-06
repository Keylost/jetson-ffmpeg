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

#ifndef ARGUS_SAMPLES_UTILS_GUI_ELEMENT_H
#define ARGUS_SAMPLES_UTILS_GUI_ELEMENT_H

#include <list>

#include "Window.h"

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkBuilder GtkBuilder;

namespace ArgusSamples
{

/**
 * A GTK widget holder
 */
class WidgetHolder
{
public:
    WidgetHolder();

    virtual ~WidgetHolder();

    GtkWidget *getWidget() const;

protected:
    WidgetHolder(GtkWidget *widget);
    void setWidget(GtkWidget *widget);

private:
    GtkWidget *m_widget;
};

/**
 * GUI element base
 */
class GuiElementBase : public WidgetHolder
{
public:
    GuiElementBase();
    GuiElementBase(GtkWidget *widget);
    virtual ~GuiElementBase();
};

/**
 * GUI container base
 */
class GuiContainerBase
{
public:
    GuiContainerBase();
    virtual ~GuiContainerBase();

    bool add(GtkWidget *widget, Window::IGuiElement *iGuiElement);
    bool attach(GtkWidget *widget, Window::IGuiElement *iGuiElement,
        unsigned int left, unsigned int top, unsigned int width, unsigned int height);
    bool remove(GtkWidget *widget, Window::IGuiElement *iGuiElement);
    bool clear(GtkWidget *widget);

    typedef std::list<Window::IGuiElement*>::iterator iterator;

    iterator begin()
    {
        return m_elements.begin();
    }

    iterator end()
    {
        return m_elements.end();
    }

public:
    std::list<Window::IGuiElement*> m_elements;
};

/**
 * A GUI image
 */
class GuiImage : public GuiElementBase, public Window::IGuiImage
{
public:
    GuiImage();
    GuiImage(GtkWidget *widget);
    ~GuiImage();

    bool initialize();

    /** @name IGuiImage methods */
    /**@{*/
    virtual bool set(size_t width, size_t height, uint32_t components, size_t rowStride,
        const uint8_t *data);
    /**@}*/

private:
    static gboolean drawCallback(GtkWidget *widget, cairo_t *cr, gpointer data);

    cairo_surface_t *m_surface;
};

/**
 * A GUI menu item
 */
class GuiMenuItem : public GuiElementBase, public Window::IGuiMenuItem
{
public:
    GuiMenuItem();
    GuiMenuItem(GtkWidget *widget);
    ~GuiMenuItem();

    bool initialize(const char *name, CallBackFunc function, void *userPtr);

    /** @name IGuiMenuItem methods */
    /**@{*/
    /**@}*/

private:
    CallBackFunc m_function;   //!< call back function
    void *m_userPtr;

    static void onActivate(GtkMenuItem *menuItem, gpointer data);
};

/**
 * A GUI menu
 */
class GuiMenu : public GuiElementBase, public Window::IGuiMenu
{
public:
    GuiMenu();
    GuiMenu(GtkWidget *widget);
    ~GuiMenu();

    bool initialize(const char *name);

    std::string getName() const;

    /** @name IGuiMenu methods */
    /**@{*/
    virtual bool add(Window::IGuiMenuItem *iGuiMenuItem);
    virtual bool remove(Window::IGuiMenuItem *iGuiMenuItem);
    /**@}*/

private:
    GtkWidget *m_menu;
    GuiContainerBase m_container;
};

/**
 * A GUI menu bar
 */
class GuiMenuBar : public GuiElementBase, public Window::IGuiMenuBar
{
public:
    GuiMenuBar();
    GuiMenuBar(GtkWidget *widget);
    ~GuiMenuBar();

    bool initialize();

    /** @name IGuiMenuBar methods */
    /**@{*/
    virtual bool add(Window::IGuiMenu *iGuiMenu);
    virtual bool remove(Window::IGuiMenu *iGuiMenu);
    virtual Window::IGuiMenu *getMenu(const char *name);
    /**@}*/

private:
    GuiContainerBase m_container;
};

/**
 * A GUI element
 */
class GuiElement : public GuiElementBase, public Window::IGuiElement
{
public:
    GuiElement()
    {
    }

    GuiElement(GtkWidget *widget)
        : GuiElementBase(widget)
    {
    }

    virtual ~GuiElement()
    {
    }
};

/**
 * GUI container
 */
class GuiContainer : public GuiElementBase, public Window::IGuiContainer
{
public:
    GuiContainer();
    GuiContainer(GtkWidget *widget);
    ~GuiContainer();

    /** @name IGuiContainer methods */
    /**@{*/
    virtual bool add(IGuiElement *iGuiElement);
    virtual bool remove(IGuiElement *iGuiElement);
    /**@}*/

private:
    GuiContainerBase m_container;
};

/**
 * A GUI container organized as a grid
 */
class GuiContainerGrid : public GuiElementBase, public Window::IGuiContainerGrid
{
public:
    GuiContainerGrid();
    ~GuiContainerGrid();

    bool initialize();

    /** @name IGuiContainer methods */
    /**@{*/
    virtual bool add(IGuiElement *iGuiElement);
    virtual bool remove(IGuiElement *iGuiElement);
    /**@}*/

    /** @name IGuiContainerGrid methods */
    /**@{*/
    virtual bool attach(IGuiElement *iGuiElement, unsigned int left, unsigned int top,
        unsigned int width = 1, unsigned int height = 1);
    /**@}*/

private:
    GuiContainerBase m_container;
};

/**
 * A GUI builder is a special container initialized with a builder string.
 */
class GuiBuilder : public GuiElementBase, public Window::IGuiBuilder
{
public:
    explicit GuiBuilder(const char *builderString);
    ~GuiBuilder();

    bool initialize();

    /** @name IGuiContainer methods */
    /**@{*/
    virtual bool add(IGuiElement *iGuiElement);
    virtual bool remove(IGuiElement *iGuiElement);
    /**@}*/

    /** @name IGuiBuilder methods */
    /**@{*/
    virtual IGuiElement *createElement(const char *name);
    /**@}*/

private:
    const char *m_builderString;
    GtkBuilder *m_builder;

    GuiBuilder();
};

}; // namespace ArgusSamples

#endif // ARGUS_SAMPLES_UTILS_GUI_ELEMENT_H
