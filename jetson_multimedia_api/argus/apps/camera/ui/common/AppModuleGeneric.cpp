/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION. All rights reserved.
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
#include <string.h>

#include "AppModuleGeneric.h"
#include "XMLConfig.h"
#include "Dispatcher.h"
#include "Error.h"
#include "Options.h"
#include "Window.h"

#include <Argus/Ext/DeFog.h>

namespace ArgusSamples
{

/**
 * Default configuration file name
 */
#define DEFAULT_CONFIG_FILE "argusAppConfig.xml"

/* static */ bool AppModuleGeneric::info(void *userPtr, const char *optArg)
{
    std::string info;
    PROPAGATE_ERROR(Dispatcher::getInstance().getInfo(info));
    printf("%s\n", info.c_str());

    return true;
}

/* static */ bool AppModuleGeneric::loadConfig(void *userPtr, const char *optArg)
{
    /// @todo ask for file if called from GUI

    const char *configFile = DEFAULT_CONFIG_FILE;
    if (optArg)
        configFile = optArg;
    PROPAGATE_ERROR(ArgusSamples::loadConfig(configFile));
    return true;
}

/* static */ bool AppModuleGeneric::saveConfig(void *userPtr, const char *optArg)
{
    /// @todo ask for file if called from GUI

    const char *configFile = DEFAULT_CONFIG_FILE;
    if (optArg)
        configFile = optArg;
    PROPAGATE_ERROR(ArgusSamples::saveConfig(configFile));
    return true;
}

/* static */ bool AppModuleGeneric::quit(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Window::getInstance().requestExit());
    return true;
}

AppModuleGeneric::AppModuleGeneric()
    : m_initialized(false)
    , m_running(false)
    , m_guiMenuBar(NULL)
    , m_guiContainerConfig(NULL)
    , m_guiConfig(NULL)
{
}

AppModuleGeneric::~AppModuleGeneric()
{
    shutdown();
}

bool AppModuleGeneric::initialize(Options &options)
{
    if (m_initialized)
        return true;

    PROPAGATE_ERROR(options.addDescription(
        "The supported value range of some settings is device or sensor mode dependent.\n"
        "Use the '--info' option to get a list of the supported values.\n"));

    PROPAGATE_ERROR(options.addOption(
        Options::Option("info", 'i', "",
            Options::Option::TYPE_ACTION,"print information on devices.", info)));

    PROPAGATE_ERROR(options.addOption(
        Options::Option("loadconfig", 0, "FILE",
            Options::Option::TYPE_ACTION, "load configuration from XML FILE. ",
            loadConfig, this, DEFAULT_CONFIG_FILE)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("saveconfig", 0, "FILE",
            Options::Option::TYPE_ACTION, "save configuration to XML FILE. ",
            saveConfig, this, DEFAULT_CONFIG_FILE)));

    PROPAGATE_ERROR(options.addOption(
        createValueOption("verbose", 0, "0 or 1", "enable verbose mode.",
            Dispatcher::getInstance().m_verbose, "1")));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("kpi", 0, "0 or 1", "enable kpi mode.",
            Dispatcher::getInstance().m_kpi, "1")));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("device", 'd', "INDEX", "select camera device with INDEX.",
            Dispatcher::getInstance().m_deviceIndex)));

    // source settings
    PROPAGATE_ERROR(options.addOption(
        createValueOption("exposuretimerange", 0, "RANGE",
            "sets the exposure time range to RANGE, in nanoseconds.",
            Dispatcher::getInstance().m_exposureTimeRange)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("gainrange", 0, "RANGE", "sets the gain range to RANGE.",
            Dispatcher::getInstance().m_gainRange)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("sensormode", 0, "INDEX", "set sensor mode to INDEX.",
            Dispatcher::getInstance().m_sensorModeIndex)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("framerate", 0, "RATE",
            "set the sensor frame rate to RATE. If RATE is 0 then VFR (variable frame rate) is "
            "enabled.", Dispatcher::getInstance().m_frameRate)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("focusposition", 0, "POSITION",
            "sets the focus position to POSITION, in focuser units.",
            Dispatcher::getInstance().m_focusPosition)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("apertureposition", 0, "POSITION",
            "sets the aperture position to POSITION, in position units.",
            Dispatcher::getInstance().m_aperturePosition)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("apertureFnum", 0, "Fnum",
            "sets the aperture F-num, in F-num units.",
            Dispatcher::getInstance().m_apertureFnum)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("aperturemotorspeed", 0, "SPEED",
            "sets the aperture motor speed to SPEED, in steps/second units.",
            Dispatcher::getInstance().m_apertureMotorSpeed)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("captureyuvformat", 0, "FORMAT",
            "YUV format for image capture.", Dispatcher::getInstance().m_captureYuvFormat)));

    // output settings
    PROPAGATE_ERROR(options.addOption(
        createValueOption("outputsize", 0, "WIDTHxHEIGHT",
            "set the still and video output size to WIDTHxHEIGHT (e.g. 1920x1080). If WIDTHxHEIGHT "
            "is '0x0' the output size is the sensor mode size.",
            Dispatcher::getInstance().m_outputSize)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("outputpath", 0, "PATH",
            "set the output file path. A file name, an incrementing index and the file extension"
            " will be appended. E.g. setting 'folder/' will result in 'folder/image0.jpg' or "
            "'folder/video0.mp4'. '/dev/null' can be used to discard output.",
            Dispatcher::getInstance().m_outputPath)));

    // stream settings
    PROPAGATE_ERROR(options.addOption(
        createValueOption("denoise", 0, "MODE", "set the denoising mode.",
            Dispatcher::getInstance().m_denoiseMode)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("denoisestrength", 0, "POSITION", "set the denoising strength.",
            Dispatcher::getInstance().m_denoiseStrength)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("edgeenhance", 0, "MODE", "set the edge enhancement mode.",
            Dispatcher::getInstance().m_edgeEnhanceMode)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("edgeenhancestrength", 0, "POSITION",
            "set the edge enhancement strength.",
            Dispatcher::getInstance().m_edgeEnhanceStrength)));

    // auto control settings
    PROPAGATE_ERROR(options.addOption(
        createValueOption("aeantibanding", 0, "MODE", "set the auto exposure antibanding mode.",
            Dispatcher::getInstance().m_aeAntibandingMode)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("aelock", 0, "LOCK", "set the auto exposure lock.",
            Dispatcher::getInstance().m_aeLock)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("awblock", 0, "LOCK", "set the auto white balance lock.",
            Dispatcher::getInstance().m_awbLock)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("awb", 0, "MODE", "set the auto white balance mode.",
            Dispatcher::getInstance().m_awbMode)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("exposurecompensation", 0, "COMPENSATION",
            "set the exposure compensation to COMPENSATION.",
            Dispatcher::getInstance().m_exposureCompensation)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("ispdigitalgainrange", 0, "RANGE",
            "sets the ISP digital gain range.",
            Dispatcher::getInstance().m_ispDigitalGainRange)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("acregionhorizontal", 0, "RANGE",
            "sets the AC auto control region horizontal range.",
            Dispatcher::getInstance().m_acRegionHorizontal)));
    PROPAGATE_ERROR(options.addOption(
        createValueOption("acregionvertical", 0, "RANGE",
            "sets the AC auto control region vertical range.",
            Dispatcher::getInstance().m_acRegionVertical)));

    if (Dispatcher::getInstance().supportsExtension(Argus::EXT_DE_FOG))
    {
        // DeFog extension settings
        PROPAGATE_ERROR(options.addOption(
            createValueOption("defog", 0, "ENABLE",
                "set the DeFog enable flag to ENABLE.",
                Dispatcher::getInstance().m_deFogEnable)));
        PROPAGATE_ERROR(options.addOption(
            createValueOption("defogamount", 0, "AMOUNT",
                "sets the amount of fog to be removed to AMOUNT.",
                Dispatcher::getInstance().m_deFogAmount)));
        PROPAGATE_ERROR(options.addOption(
            createValueOption("defogquality", 0, "QUALITY",
                "sets the quality of the DeFog effect to QUALITY.",
                Dispatcher::getInstance().m_deFogQuality)));
    }

    m_initialized = true;

    return true;
}

bool AppModuleGeneric::shutdown()
{
    if (!m_initialized)
        return true;

    PROPAGATE_ERROR_CONTINUE(stop());

    delete m_guiConfig;
    m_guiConfig = NULL;

    m_initialized = false;

    return true;
}

bool AppModuleGeneric::start(Window::IGuiMenuBar *iGuiMenuBar,
    Window::IGuiContainer *iGuiContainerConfig)
{
    if (m_running)
        return true;

    if (iGuiMenuBar && !m_guiMenuBar)
    {
        // initialize the menu

        UniquePointer<Window::IGuiMenu> menu;
        UniquePointer<Window::IGuiMenuItem> item;

        // create the elements
        Window::IGuiMenu *fileMenu = iGuiMenuBar->getMenu("File");
        if (!fileMenu)
        {
            PROPAGATE_ERROR(Window::IGuiMenu::create("File", &menu));
            PROPAGATE_ERROR(iGuiMenuBar->add(menu.get()));
            fileMenu = menu.get();
            menu.release();
        }
        PROPAGATE_ERROR(Window::IGuiMenuItem::create("Load config", AppModuleGeneric::loadConfig,
            NULL, &item));
        PROPAGATE_ERROR(fileMenu->add(item.get()));
        item.release();
        PROPAGATE_ERROR(Window::IGuiMenuItem::create("Save Config", AppModuleGeneric::saveConfig,
            NULL, &item));
        PROPAGATE_ERROR(fileMenu->add(item.get()));
        item.release();
        PROPAGATE_ERROR(Window::IGuiMenuItem::create("Quit", AppModuleGeneric::quit, NULL,
            &item));
        PROPAGATE_ERROR(fileMenu->add(item.get()));
        item.release();

        Window::IGuiMenu *helpMenu = iGuiMenuBar->getMenu("Help");
        if (!helpMenu)
        {
            PROPAGATE_ERROR(Window::IGuiMenu::create("Help", &menu));
            PROPAGATE_ERROR(iGuiMenuBar->add(menu.get()));
            helpMenu = menu.get();
            menu.release();
        }
        PROPAGATE_ERROR(Window::IGuiMenuItem::create("Info", AppModuleGeneric::info, NULL,
            &item));
        PROPAGATE_ERROR(helpMenu->add(item.get()));
        item.release();

        m_guiMenuBar = iGuiMenuBar;
    }

    if (iGuiContainerConfig && !m_guiContainerConfig)
    {
        // initialize the GUI

        // create a grid container
        PROPAGATE_ERROR(Window::IGuiContainerGrid::create(&m_guiConfig));

        // create the elements
        UniquePointer<Window::IGuiElement> element;
        Dispatcher &dispatcher = Dispatcher::getInstance();

        Window::IGuiContainerGrid::BuildHelper buildHelper(m_guiConfig);

#define CREATE_GUI_ELEMENT(_NAME, _VALUE)                                           \
        PROPAGATE_ERROR(Window::IGuiElement::createValue(&dispatcher._VALUE, &element));\
        PROPAGATE_ERROR(buildHelper.append(_NAME, element.get()));                  \
        element.release();

#define CREATE_GUI_ELEMENT_COMBO_BOX(_NAME, _VALUE, _FROMTYPE, _TOTYPE)             \
        assert(sizeof(_FROMTYPE) == sizeof(_TOTYPE));                               \
        PROPAGATE_ERROR(Window::IGuiElement::createValue(reinterpret_cast<          \
            Value<_TOTYPE>*>(&dispatcher._VALUE), &element));                       \
        PROPAGATE_ERROR(buildHelper.append(_NAME, element.get()));                  \
        element.release();

#define CREATE_GUI_ELEMENT_PATH_CHOOSER(_NAME, _VALUE)                              \
        PROPAGATE_ERROR(Window::IGuiElement::createFileChooser(&dispatcher._VALUE,  \
            true, &element));                                                       \
        PROPAGATE_ERROR(buildHelper.append(_NAME, element.get()));                  \
        element.release();

        CREATE_GUI_ELEMENT("Verbose", m_verbose);
        CREATE_GUI_ELEMENT("KPI", m_kpi);
        CREATE_GUI_ELEMENT("Device", m_deviceIndex);

        CREATE_GUI_ELEMENT("Exposure time range (ns)", m_exposureTimeRange);
        CREATE_GUI_ELEMENT("Gain range", m_gainRange);
        CREATE_GUI_ELEMENT("ISP digital gain range", m_ispDigitalGainRange);
        CREATE_GUI_ELEMENT("AC region horizontal", m_acRegionHorizontal);
        CREATE_GUI_ELEMENT("AC region vertical", m_acRegionVertical);

        CREATE_GUI_ELEMENT_COMBO_BOX("Sensor mode index", m_sensorModeIndex,
            uint32_t, Window::IGuiElement::ValueTypeEnum);
        CREATE_GUI_ELEMENT_COMBO_BOX("YUV format", m_captureYuvFormat,
            Argus::PixelFormat, Argus::NamedUUID);
        CREATE_GUI_ELEMENT("Frame rate", m_frameRate);
        CREATE_GUI_ELEMENT("Focus position", m_focusPosition);
        CREATE_GUI_ELEMENT("Aperture position", m_aperturePosition);
        CREATE_GUI_ELEMENT_COMBO_BOX("Aperture Fnum", m_apertureFnum,
            float, Window::IGuiElement::ValueTypeEnum);
        CREATE_GUI_ELEMENT("Aperture motor speed", m_apertureMotorSpeed);
        CREATE_GUI_ELEMENT("Output size", m_outputSize);

        CREATE_GUI_ELEMENT_PATH_CHOOSER("Output path", m_outputPath);

        CREATE_GUI_ELEMENT_COMBO_BOX("De-Noise mode", m_denoiseMode,
            Argus::DenoiseMode, Argus::NamedUUID);
        CREATE_GUI_ELEMENT("De-Noise strength", m_denoiseStrength);
        CREATE_GUI_ELEMENT_COMBO_BOX("Edge Enhance mode", m_edgeEnhanceMode,
            Argus::EdgeEnhanceMode, Argus::NamedUUID);
        CREATE_GUI_ELEMENT("Edge Enhance strength", m_edgeEnhanceStrength);

        CREATE_GUI_ELEMENT_COMBO_BOX("AE antibanding mode", m_aeAntibandingMode,
            Argus::AeAntibandingMode, Argus::NamedUUID);

        CREATE_GUI_ELEMENT("AE Lock", m_aeLock);
        CREATE_GUI_ELEMENT("AWB Lock", m_awbLock);
        CREATE_GUI_ELEMENT_COMBO_BOX("AWB mode", m_awbMode,
            Argus::AwbMode, Argus::NamedUUID);
        CREATE_GUI_ELEMENT("Exposure compensation (ev)", m_exposureCompensation);

#undef CREATE_GUI_ELEMENT
#undef CREATE_GUI_ELEMENT_COMBO_BOX
#undef CREATE_GUI_ELEMENT_PATH_CHOOSER

        m_guiContainerConfig = iGuiContainerConfig;
    }

    if (m_guiContainerConfig)
        PROPAGATE_ERROR(m_guiContainerConfig->add(m_guiConfig));

    m_running = true;

    return true;
}

bool AppModuleGeneric::stop()
{
    if (!m_running)
        return true;

    if (m_guiContainerConfig)
        PROPAGATE_ERROR(m_guiContainerConfig->remove(m_guiConfig));

    m_running = true;

    return true;
}

}; // namespace ArgusSamples
