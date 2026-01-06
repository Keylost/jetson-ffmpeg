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

#include <sstream>

#include "MultiSession.h"
#include "Composer.h"
#include "Dispatcher.h"
#include "Error.h"
#include "UniquePointer.h"
#include "PerfTracker.h"
#include <algorithm>

namespace ArgusSamples
{

TaskMultiSession::TaskMultiSession()
    : m_initialized(false)
    , m_running(false)
    , m_prevRunning(false)
{
}

TaskMultiSession::~TaskMultiSession()
{
    shutdown();
}

TaskMultiSession::Session::Session()
{
}

TaskMultiSession::Session::~Session()
{
    shutdown();
}

bool TaskMultiSession::Session::initialize(uint32_t deviceIndex)
{
    // create the perf tracker
    m_perfTracker.reset(new SessionPerfTracker());
    if (!m_perfTracker)
        ORIGINATE_ERROR("Out of memory");

    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_TASK_START));

    Composer &composer = Composer::getInstance();
    Dispatcher &dispatcher = Dispatcher::getInstance();

    // create the session using the current device index
    PROPAGATE_ERROR(dispatcher.createSession(m_session, deviceIndex));
    PROPAGATE_ERROR(m_perfTracker->setSession(m_session.get()));

    // create the request
    PROPAGATE_ERROR(dispatcher.createRequest(m_request, Argus::CAPTURE_INTENT_STILL_CAPTURE,
        m_session.get()));

    // Create the preview stream
    PROPAGATE_ERROR(dispatcher.createOutputStream(m_request.get(), false,  m_outputStream,
        m_session.get()));

    // bind the preview stream to the composer
    Argus::IEGLOutputStream *iEGLOutputStream =
        Argus::interface_cast<Argus::IEGLOutputStream>(m_outputStream.get());
    if (!iEGLOutputStream)
        ORIGINATE_ERROR("Failed to get IEGLOutputStream interface");

    // Bind the stream to the composer
    PROPAGATE_ERROR(composer.bindStream(iEGLOutputStream->getEGLStream()));

    const Argus::Size2D<uint32_t> streamSize = iEGLOutputStream->getResolution();
    PROPAGATE_ERROR(composer.setStreamAspectRatio(iEGLOutputStream->getEGLStream(),
        (float)streamSize.width() / (float)streamSize.height()));

    // Enable the output stream
    PROPAGATE_ERROR(dispatcher.enableOutputStream(m_request.get(), m_outputStream.get()));

    return true;
}

bool TaskMultiSession::Session::start()
{
    Composer &composer = Composer::getInstance();

    // activate the streams and populate the burst request array
    PROPAGATE_ERROR(composer.setStreamActive(
        Argus::interface_cast<Argus::IEGLOutputStream>(m_outputStream)->getEGLStream(), true));

    // start the repeating burst request for the preview
    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_ISSUE_CAPTURE));
    PROPAGATE_ERROR(Dispatcher::getInstance().startRepeat(m_request.get(), m_session.get()));

    return true;
}

bool TaskMultiSession::Session::stop()
{
    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_CLOSE_REQUESTED));

    Dispatcher &dispatcher = Dispatcher::getInstance();
    Composer &composer = Composer::getInstance();

    PROPAGATE_ERROR(dispatcher.stopRepeat(m_session.get()));
    PROPAGATE_ERROR(composer.setStreamActive(
        Argus::interface_cast<Argus::IEGLOutputStream>(m_outputStream)->getEGLStream(), false));
    PROPAGATE_ERROR(dispatcher.waitForIdle(m_session.get()));
    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_FLUSH_DONE));

    return true;
}

bool TaskMultiSession::Session::shutdown()
{
    if (m_request)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();
        Composer &composer = Composer::getInstance();

        if (m_outputStream)
        {
            // destroy the producer
            PROPAGATE_ERROR_CONTINUE(dispatcher.disableOutputStream(m_request.get(),
                m_outputStream.get()));

            Argus::IEGLOutputStream *iEGLOutputStream =
                Argus::interface_cast<Argus::IEGLOutputStream>(m_outputStream);
            if (!iEGLOutputStream)
                REPORT_ERROR("Failed to get IEGLOutputStream interface");

            // disconnect the EGL stream
            iEGLOutputStream->disconnect();

            // unbind the EGL stream from the composer
            PROPAGATE_ERROR_CONTINUE(composer.unbindStream(iEGLOutputStream->getEGLStream()));

            m_outputStream.reset();
        }

        // destroy the request
        PROPAGATE_ERROR_CONTINUE(m_request.reset());
    }

    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_CLOSE_DONE));

    // Destroy the session
    m_session.reset();

    return true;
}

bool TaskMultiSession::initialize()
{
    if (m_initialized)
        return true;

    PROPAGATE_ERROR(Dispatcher::getInstance().m_sensorModeValid.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskMultiSession::onSensorModeValidChanged)));
    PROPAGATE_ERROR(Dispatcher::getInstance().m_outputSize.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskMultiSession::restartStreams)));

    m_initialized = true;

    return true;
}

bool TaskMultiSession::shutdown()
{
    if (!m_initialized)
        return true;

    // stop the preview
    PROPAGATE_ERROR(stop());

    PROPAGATE_ERROR_CONTINUE(Dispatcher::getInstance().m_outputSize.unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskMultiSession::restartStreams)));
    PROPAGATE_ERROR_CONTINUE(Dispatcher::getInstance().m_sensorModeValid.unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskMultiSession::onSensorModeValidChanged)));

    m_initialized = false;

    return true;
}

bool TaskMultiSession::shutdownSessions()
{
    if (!m_sessions.empty())
    {
        // shutdown the sessions
        for (std::list<Session*>::iterator it = m_sessions.begin(); it != m_sessions.end(); ++it)
        {
            Session *session = *it;
            PROPAGATE_ERROR_CONTINUE(session->shutdown());
            delete session;
        }
        m_sessions.clear();
    }

    return true;
}

bool TaskMultiSession::start()
{
    if (m_running)
        return true;

    Dispatcher &dispatcher = Dispatcher::getInstance();

    if (m_sessions.empty())
    {
        const uint32_t deviceCount = dispatcher.getDeviceCount();

        if (deviceCount == 0)
            ORIGINATE_ERROR("No camera devices found");

        std::vector<uint32_t> devices;

        if (m_multiDevices.get().size() > 0)
        {
            // m_multiDevices will not be changed by UI
            // it has special validation requirements, so validate m_multiDevices here
            devices = m_multiDevices.get();
            std::sort(devices.begin(), devices.end());

            // compare with deviceCount
            if (devices.back() >= deviceCount)
                ORIGINATE_ERROR("index %u is out of range [0 - %u)", devices.back(), deviceCount);

            // check no duplicate
            std::vector<uint32_t>::iterator it = std::unique(devices.begin(), devices.end());
            if (it != devices.end())
                ORIGINATE_ERROR("duplicated indexes");
        }
        else
        {
            // use all available camera devices
            for (uint32_t deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex)
            {
                devices.push_back(deviceIndex);
            }
        }

        // create a request and streams for each session
        for (std::vector<uint32_t>::iterator it = devices.begin(); it != devices.end(); ++it)
        {
            UniquePointer<Session> session(new Session);

            if (!session)
                ORIGINATE_ERROR("Out of memory");

            PROPAGATE_ERROR(session->initialize(*it));

            m_sessions.push_back(session.release());
        }
    }

    // start the sessions
    for (std::list<Session*>::iterator it = m_sessions.begin(); it != m_sessions.end(); ++it)
    {
        Session *session = *it;
        PROPAGATE_ERROR(session->start());
    }

    m_running = true;

    return true;
}

bool TaskMultiSession::stop()
{
    if (!m_running)
        return true;

    for (std::list<Session*>::iterator it = m_sessions.begin(); it != m_sessions.end(); ++it)
    {
        Session *session = *it;
        PROPAGATE_ERROR(session->stop());
    }

    PROPAGATE_ERROR(shutdownSessions());

    m_running = false;

    return true;
}

bool TaskMultiSession::onSensorModeValidChanged(const Observed &source)
{
    const bool isTrue = static_cast<const Value<bool>&>(source).get();

    if (!isTrue)
    {
        m_prevRunning = m_running;
        if (m_running)
        {
            PROPAGATE_ERROR(stop());
        }
    }
    else if (m_prevRunning)
    {
        m_prevRunning = false;
        PROPAGATE_ERROR(start());
    }

    return true;
}

bool TaskMultiSession::restartStreams(__attribute__((unused)) const Observed &source)
{
    if (m_running)
    {
        PROPAGATE_ERROR(stop());
        PROPAGATE_ERROR(start());
    }
    return true;
}

}; // namespace ArgusSamples
