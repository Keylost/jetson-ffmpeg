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

#include "App.h"
#include "Error.h"

#include "Dispatcher.h"
#include "Composer.h"
#include "PerfTracker.h"

namespace ArgusSamples
{

App::App(const char *appName)
    : m_options(appName)
{
}

App::~App()
{
    shutdown();
}

bool App::initialize()
{
    PROPAGATE_ERROR(Window::getInstance().registerObserver(this));

    const char *description =
        "Press 'Ctrl-Up' to increase the focus position, press 'Ctrl-Down' to decrease the focus\n"
        "position.\n"
        "Press 'd' to dump runtime information.\n"
        "Press 'Esc' to exit.\n";
    PROPAGATE_ERROR(m_options.addDescription(description));

    return true;
}

bool App::shutdown()
{
    PROPAGATE_ERROR(Window::getInstance().unregisterObserver(this));

    // shutdown the composer
    PROPAGATE_ERROR(Composer::getInstance().shutdown());

    // shutdown the window
    PROPAGATE_ERROR(Window::getInstance().shutdown());

    // shutdown the dispatcher
    PROPAGATE_ERROR(Dispatcher::getInstance().shutdown());

    return true;
}

bool App::run(int argc, char **argv)
{
    PROPAGATE_ERROR(PerfTracker::getInstance().onEvent(GLOBAL_EVENT_APP_START));

    PROPAGATE_ERROR(initialize());

    PROPAGATE_ERROR(PerfTracker::getInstance().onEvent(GLOBAL_EVENT_APP_INITIALIZED));

    // parse and execute the options
    PROPAGATE_ERROR(m_options.parse(argc, argv));

    // if exit had not been requested start the window event loop
    if (!m_options.requestedExit())
    {
        Window &window = Window::getInstance();

        // start the active module
        PROPAGATE_ERROR(start());

        // start the event loop
        PROPAGATE_ERROR(window.eventLoop());
    }

    return true;
}

/**
 * Moves the focus position by one percent in 'direction'
 * @param [in] direction either '-1' to move focus position down, or '+1' to move it up
 */
static bool changeFocusPosition(int32_t direction)
{
    Dispatcher &dispatcher = Dispatcher::getInstance();
    const Argus::Range<int32_t> focusPositionRange = dispatcher.getDeviceFocusPositionRange();

    if ((direction != -1) && (direction != 1))
        ORIGINATE_ERROR("Invalid direction");

    const int32_t diff = ((focusPositionRange.max() - focusPositionRange.min()) + 99) / 100;

    int32_t newPosition = dispatcher.m_focusPosition.get() + diff * direction;

    newPosition =
        std::min(focusPositionRange.max(), std::max(focusPositionRange.min(), newPosition));

    PROPAGATE_ERROR(dispatcher.m_focusPosition.set(newPosition));

    PROPAGATE_ERROR(dispatcher.message("Changed focuser position to %d in range [%d, %d]\n",
        newPosition, focusPositionRange.min(), focusPositionRange.max()));

    return true;
}

/**
 * Moves the aperture positions by one percent in 'direction'
 * @param [in] direction either '-1' to move aperture position down, or '+1' to move it up
 */
static bool changeAperturePosition(int32_t direction)
{
    Dispatcher &dispatcher = Dispatcher::getInstance();
    const Argus::Range<int32_t> aperturePositionRange = dispatcher.getDeviceAperturePositionRange();

    if ((direction != -1) && (direction != 1))
        ORIGINATE_ERROR("Invalid direction");

    int32_t newStep = dispatcher.m_aperturePosition.get() + direction;

    newStep =
            std::min(aperturePositionRange.max(), std::max(aperturePositionRange.min(), newStep));

    PROPAGATE_ERROR(dispatcher.m_aperturePosition.set(newStep));

    PROPAGATE_ERROR(dispatcher.message("Changed aperture position to %d in range [%d, %d]\n",
        newStep, aperturePositionRange.min(), aperturePositionRange.max()));

    return true;
}

bool App::onKey(const Key &key)
{
    if ((key == Key("Escape")) ||
        (key == Key("c", KeyModifier(KeyModifier::MASK_CONTROL))))
    {
        PROPAGATE_ERROR(Window::getInstance().requestExit());
    }
    else if (key == Key("d"))
    {
        Dispatcher::getInstance().dumpSessionInfo();
    }
    else if (key == Key("Up", KeyModifier(KeyModifier::MASK_CONTROL)))
    {
        PROPAGATE_ERROR(changeFocusPosition(+1));
    }
    else if (key == Key("Down", KeyModifier(KeyModifier::MASK_CONTROL)))
    {
        PROPAGATE_ERROR(changeFocusPosition(-1));
    }
    else if (key == Key("Left", KeyModifier(KeyModifier::MASK_CONTROL)))
    {
        PROPAGATE_ERROR(changeAperturePosition(+1));
    }
    else if (key == Key("Right", KeyModifier(KeyModifier::MASK_CONTROL)))
    {
        PROPAGATE_ERROR(changeAperturePosition(-1));
    }

    // silently ignore unhandled keys
    return true;
}

}; // namespace ArgusSamples

