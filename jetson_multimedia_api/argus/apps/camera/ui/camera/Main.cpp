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

// For Bug 200239385 and Bug 200243017
// There are asserts while operating the gallery.
// With the following #define commented out, access to the gallery is disabled until it can be fixed
//#define GALLERY_SUPPORTED

#include <stdlib.h>

#include <list>

#include "Error.h"
#include "UniquePointer.h"
#include "Window.h"
#include "Value.h"
#include "Validator.h"
#include "IObserver.h"

#include "App.h"
#include "AppModuleCapture.h"
#include "AppModuleVideo.h"
#include "AppModuleMultiExposure.h"
#include "AppModuleMultiSession.h"
#include "AppModuleGallery.h"
#include "AppModuleGeneric.h"

namespace ArgusSamples
{

#if (WINDOW_GUI_SUPPORT == WINDOW_GUI_GTK)
/**
 * GTK UI builder string
 */
static const char builderString[] =
{
#include "cameraBuilder.h"
    , 0x00
};
#endif // (WINDOW_GUI_SUPPORT == WINDOW_GUI_GTK)

/**
 * Supported modules
 */
enum Modules
{
    MODULE_CAPTURE,
    MODULE_VIDEO,
    MODULE_MULTI_EXPOSURE,
    MODULE_MULTI_SESSION,
#ifdef GALLERY_SUPPORTED
    MODULE_GALLERY,
#endif
    MODULE_FIRST = MODULE_CAPTURE,
#ifdef GALLERY_SUPPORTED
    MODULE_LAST = MODULE_GALLERY,
#else
    MODULE_LAST = MODULE_MULTI_SESSION,
#endif
    MODULE_COUNT,
    MODULE_INVALID = -1
};

// valid module values
static const ValidatorEnum<Modules>::ValueStringPair s_modules[] =
{
    { MODULE_CAPTURE, "Capture" },
    { MODULE_VIDEO, "Video" },
    { MODULE_MULTI_EXPOSURE, "Multi Exposure" },
#ifdef GALLERY_SUPPORTED
    { MODULE_MULTI_SESSION, "Multi Session" },
    { MODULE_GALLERY, "Gallery" }
#else
    { MODULE_MULTI_SESSION, "Multi Session" }
#endif
};

class CameraApp : public App, public IObserver
{
public:
    explicit CameraApp(const char *appName);
    ~CameraApp();

    /** @name App methods */
    /**@{*/
    virtual bool initialize();
    virtual bool shutdown();
    virtual bool start();
    /**@}*/

private:
    /**
     * Hide default constructor
     */
    CameraApp();

    /** @name IKeyObserver methods */
    /**@{*/
    virtual bool onKey(const Key &key);
    /**@}*/

    bool onModuleChanged(const Observed &source);

    AppModuleGeneric m_moduleGeneric;

    Value<Modules> m_module;                ///< active module
    Modules m_prevModule;                   ///< previously active module

    Modules m_activeModuleBeforeGallery;    ///< active module when switching to gallery

    std::vector<IAppModule*> m_modules;     ///< all modules

    Window::IGuiMenuBar *m_iGuiMenuBar;     ///< menu bar
    Window::IGuiContainer *m_iGuiContainerConfig;   ///< container for config GUI elements
};

CameraApp::CameraApp(const char *appName)
    : App(appName)
    , m_module(new ValidatorEnum<Modules>(
        s_modules, sizeof(s_modules) / sizeof(s_modules[0])),
        MODULE_FIRST)
    , m_prevModule(MODULE_INVALID)
    , m_activeModuleBeforeGallery(MODULE_INVALID)
    , m_iGuiMenuBar(NULL)
    , m_iGuiContainerConfig(NULL)
{
}

CameraApp::~CameraApp()
{
    shutdown();
}

bool CameraApp::initialize()
{
    PROPAGATE_ERROR(App::initialize());

    const char *description =
        "Press 'm' to toggle between modules (still capture, video recording, multi exposure, \n"
        "multi session).\n"
#ifdef GALLERY_SUPPORTED
        "Press 'g' to switch to gallery and back. Use left and right arrow keys to move to next\n"
        "and previous image or video.\n"
#endif
        "Press 'space' to execute the module action (capture an image, start and stop recording,\n"
        "start and stop video playback.\n";
    PROPAGATE_ERROR(m_options.addDescription(description));

    PROPAGATE_ERROR(m_options.addOption(
        createValueOption("module", 0, "MODULE", "switch to module MODULE.", m_module)));

#if (WINDOW_GUI_SUPPORT == WINDOW_GUI_GTK)
    Window::IGuiBuilder *builder = NULL;
    PROPAGATE_ERROR(Window::IGuiBuilder::create(builderString, &builder));

    UniquePointer<Window::IGuiBuilder> createdBuilder(builder);
    UniquePointer<Window::IGuiElement> createdWindow(createdBuilder->createElement("window"));
    UniquePointer<Window::IGuiElement> createdView(createdBuilder->createElement("view"));

    m_iGuiMenuBar =
        static_cast<Window::IGuiMenuBar*>(createdBuilder->createElement("menuBar"));
    m_iGuiContainerConfig =
        static_cast<Window::IGuiContainer*>(createdBuilder->createElement("config"));

    // set the window GUI
    PROPAGATE_ERROR(Window::getInstance().setWindowGui(createdBuilder.get(), createdWindow.get(),
        createdView.get()));

    createdView.release();
    createdWindow.release();
    createdBuilder.release();
#endif // (WINDOW_GUI_SUPPORT == WINDOW_GUI_GTK)

    m_modules.resize(MODULE_COUNT);

    // create modules
    UniquePointer<IAppModule> module;
    module.reset(new AppModuleCapture);
    if (!module)
        ORIGINATE_ERROR("Out of memory");
    m_modules[MODULE_CAPTURE] = module.release();

    module.reset(new AppModuleVideo);
    if (!module)
        ORIGINATE_ERROR("Out of memory");
    m_modules[MODULE_VIDEO] = module.release();

    module.reset(new AppModuleMultiExposure);
    if (!module)
        ORIGINATE_ERROR("Out of memory");
    m_modules[MODULE_MULTI_EXPOSURE] = module.release();

    module.reset(new AppModuleMultiSession);
    if (!module)
        ORIGINATE_ERROR("Out of memory");
    m_modules[MODULE_MULTI_SESSION] = module.release();

#ifdef GALLERY_SUPPORTED
    module.reset(new AppModuleGallery);
    if (!module)
        ORIGINATE_ERROR("Out of memory");
    m_modules[MODULE_GALLERY] = module.release();
#endif

    // initialize generic module, this has options common to all modules
    PROPAGATE_ERROR(m_moduleGeneric.initialize(m_options));

    // initializes modules
    for (std::vector<IAppModule*>::iterator it = m_modules.begin(); it != m_modules.end(); ++it)
        PROPAGATE_ERROR((*it)->initialize(m_options));

    return true;
}

bool CameraApp::shutdown()
{
    // shutdown the modules
    for (std::vector<IAppModule*>::iterator it = m_modules.begin(); it != m_modules.end(); ++it)
    {
        PROPAGATE_ERROR((*it)->shutdown());
        delete *it;
    }
    m_modules.clear();

    PROPAGATE_ERROR(m_moduleGeneric.shutdown());

    delete m_iGuiContainerConfig;
    m_iGuiContainerConfig = NULL;

    delete m_iGuiMenuBar;
    m_iGuiMenuBar = NULL;

    PROPAGATE_ERROR(App::shutdown());

    return true;
}

bool CameraApp::start()
{
    if (m_iGuiContainerConfig)
    {
        // add GUI elements
        UniquePointer<Window::IGuiElement> element;

        assert(sizeof(Modules) == sizeof(Window::IGuiElement::ValueTypeEnum));
        PROPAGATE_ERROR(Window::IGuiElement::createValue(
            reinterpret_cast<Value<Window::IGuiElement::ValueTypeEnum>*>(&m_module), &element));
        PROPAGATE_ERROR(m_iGuiContainerConfig->add(element.get()));
        element.release();
    }

    // Start the generic module, it's always active
    PROPAGATE_ERROR(m_moduleGeneric.start(m_iGuiMenuBar, m_iGuiContainerConfig));

    // register an observer, this will stop/start modules if m_module changes
    PROPAGATE_ERROR(m_module.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&CameraApp::onModuleChanged)));

    return true;
}

bool CameraApp::onKey(const Key &key)
{
    if (key == Key("m"))
    {
        // switch to next module
        Modules curModule = m_module.get();

#ifdef GALLERY_SUPPORTED
        // switch to next module but skip gallery
        do
        {
#endif
            if (curModule == MODULE_LAST)
                curModule = MODULE_FIRST;
            else
                curModule = static_cast<Modules>(curModule + 1);
#ifdef GALLERY_SUPPORTED
        }
        while (curModule == MODULE_GALLERY);
#endif

        m_module.set(curModule);
    }
#ifdef GALLERY_SUPPORTED
    else if (key == Key("g"))
    {
        // switch to gallery on/off
        Modules curModule = m_module.get();

        if (curModule == MODULE_GALLERY)
        {
            curModule = m_activeModuleBeforeGallery;
        }
        else
        {
            m_activeModuleBeforeGallery = curModule;
            curModule = MODULE_GALLERY;
        }

        m_module.set(curModule);
    }
#endif
    // call parent
    PROPAGATE_ERROR(App::onKey(key));

    return true;
}

bool CameraApp::onModuleChanged(const Observed &source)
{
    assert(static_cast<const Value<Modules>&>(source).get() == m_module);

    if (m_prevModule != MODULE_INVALID)
        PROPAGATE_ERROR(m_modules[m_prevModule]->stop());

    m_prevModule = m_module.get();

    PROPAGATE_ERROR(m_modules[m_module.get()]->start(m_iGuiMenuBar, m_iGuiContainerConfig));

    return true;
}

}; // namespace ArgusSamples

int main(int argc, char **argv)
{
    printf("Executing Argus Sample Application (%s)\n", basename(argv[0]));

    ArgusSamples::CameraApp cameraApp(basename(argv[0]));

    if (!cameraApp.run(argc, argv))
         return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
