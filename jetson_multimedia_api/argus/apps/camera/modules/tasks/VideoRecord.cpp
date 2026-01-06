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
#include <iomanip>

#include "VideoRecord.h"
#include "Composer.h"
#include "Dispatcher.h"
#include "Error.h"
#include "EventThread.h"
#include "PerfTracker.h"

namespace ArgusSamples
{

TaskVideoRecord::TaskVideoRecord()
    : m_initialized(false)
    , m_running(false)
    , m_wasRunning(false)
    , m_prevRunning(false)
    , m_recording(false)
    , m_captureIndex(0)
    , m_videoPipeline(NULL)
{
}

TaskVideoRecord::~TaskVideoRecord()
{
    shutdown();
}

bool TaskVideoRecord::initialize()
{
    if (m_initialized)
        return true;

    Dispatcher &dispatcher = Dispatcher::getInstance();

    PROPAGATE_ERROR(dispatcher.m_deviceOpen.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskVideoRecord::onDeviceOpenChanged)));
    PROPAGATE_ERROR(dispatcher.m_sensorModeValid.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskVideoRecord::onSensorModeValidChanged)));
    PROPAGATE_ERROR(dispatcher.m_outputSize.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskVideoRecord::restartStreams)));
    PROPAGATE_ERROR(dispatcher.m_captureYuvFormat.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskVideoRecord::restartStreams)));

    m_perfTracker.reset(new SessionPerfTracker());
    if (!m_perfTracker)
        ORIGINATE_ERROR("Out of memory");

    m_initialized = true;

    return true;
}

bool TaskVideoRecord::restartStreams(__attribute__((unused)) const Observed &source)
{
    if (m_running)
    {
        PROPAGATE_ERROR(stop());
        PROPAGATE_ERROR(start());
    }
    return true;
}

bool TaskVideoRecord::onDeviceOpenChanged(const Observed &source)
{
    const bool isOpen = static_cast<const Value<bool>&>(source).get();

    // If the current device is closed the request needs to be recreated on the new device. Stop
    // and then start when the device is opened again.
    if (!isOpen)
    {
        m_wasRunning = m_running;
        PROPAGATE_ERROR(stop());
    }
    else if (m_wasRunning)
    {
        m_wasRunning = false;
        PROPAGATE_ERROR(start());
    }

    return true;
}

bool TaskVideoRecord::onSensorModeValidChanged(const Observed &source)
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

bool TaskVideoRecord::start()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Not initialized");

    if (m_running)
        return true;

    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_TASK_START));

    Dispatcher &dispatcher = Dispatcher::getInstance();
    Composer &composer = Composer::getInstance();

    PROPAGATE_ERROR(dispatcher.createRequest(m_request, Argus::CAPTURE_INTENT_VIDEO_RECORD));

    // Create the preview stream
    PROPAGATE_ERROR(dispatcher.createOutputStream(m_request.get(), false, m_previewStream));

    // bind the preview stream to the composer
    Argus::IEGLOutputStream *iEGLOutputStream =
        Argus::interface_cast<Argus::IEGLOutputStream>(m_previewStream.get());
    if (!iEGLOutputStream)
        ORIGINATE_ERROR("Failed to get IEGLOutputStream interface");

    PROPAGATE_ERROR(composer.bindStream(iEGLOutputStream->getEGLStream()));

    // Enable the preview stream
    PROPAGATE_ERROR(dispatcher.enableOutputStream(m_request.get(), m_previewStream.get()));

    const Argus::Size2D<uint32_t> streamSize = iEGLOutputStream->getResolution();
    PROPAGATE_ERROR(composer.setStreamAspectRatio(iEGLOutputStream->getEGLStream(),
        (float)streamSize.width() / (float)streamSize.height()));
    PROPAGATE_ERROR(composer.setStreamActive(iEGLOutputStream->getEGLStream(), true));

    // start the preview
    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_ISSUE_CAPTURE));
    PROPAGATE_ERROR(dispatcher.startRepeat(m_request.get()));

    m_running = true;

    return true;
}

bool TaskVideoRecord::stop()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Not initialized");
    if (!m_running)
        return true;

    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_CLOSE_REQUESTED));

    if (m_recording)
        PROPAGATE_ERROR(stopRecording());

    Dispatcher &dispatcher = Dispatcher::getInstance();

    // stop the repeating request
    PROPAGATE_ERROR(dispatcher.stopRepeat());

    PROPAGATE_ERROR(dispatcher.waitForIdle());
    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_FLUSH_DONE));

    if (m_previewStream)
    {
        if (m_request)
            PROPAGATE_ERROR(dispatcher.disableOutputStream(m_request.get(), m_previewStream.get()));

        Argus::IEGLOutputStream *iEGLOutputStream =
            Argus::interface_cast<Argus::IEGLOutputStream>(m_previewStream);
        if (!iEGLOutputStream)
            ORIGINATE_ERROR("Failed to get IEGLOutputStream interface");

        // disconnect the EGL stream
        iEGLOutputStream->disconnect();

        // unbind the preview stream from the composer
        PROPAGATE_ERROR(Composer::getInstance().unbindStream(iEGLOutputStream->getEGLStream()));

        m_previewStream.reset();
    }
    PROPAGATE_ERROR_CONTINUE(m_request.reset());

    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_CLOSE_DONE));

    m_running = false;

    return true;
}

bool TaskVideoRecord::startRecording()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Not initialized");
    if (!m_running)
        ORIGINATE_ERROR("Not running");
    if (m_recording)
        ORIGINATE_ERROR("Recording had already been started, can't start again");

    Dispatcher &dispatcher = Dispatcher::getInstance();

    // setup the video pipeline with the video stream
    m_videoPipeline = new VideoPipeline;
    if (!m_videoPipeline)
        ORIGINATE_ERROR("Out of memory");

    // Create the video output stream
    PROPAGATE_ERROR(dispatcher.createOutputStream(m_request.get(), false, m_videoStream));

    Argus::Size2D<uint32_t> outputSize;
    PROPAGATE_ERROR(dispatcher.getOutputSize(&outputSize));

    // build the file name
    std::ostringstream fileName;
    fileName << dispatcher.m_outputPath.get();
    if (dispatcher.m_outputPath.get() != "/dev/null")
        fileName << "/video" << std::setfill('0') << std::setw(4) << m_captureIndex;
    ++m_captureIndex;

    PROPAGATE_ERROR(m_videoPipeline->setupForRecording(
        Argus::interface_cast<Argus::IEGLOutputStream>(m_videoStream)->getEGLStream(),
        outputSize.width(), outputSize.height(),
        dispatcher.m_frameRate.get(), fileName.str().c_str(), dispatcher.m_videoFormat.get(),
        dispatcher.m_videoFileType.get(), dispatcher.m_videoBitRate.get()));

    // start recording
    PROPAGATE_ERROR(m_videoPipeline->start());

    // Enable the video output stream
    PROPAGATE_ERROR(dispatcher.enableOutputStream(m_request.get(), m_videoStream.get()));

    // restart the repeating request to ensure the changed request is executed
    PROPAGATE_ERROR(dispatcher.startRepeat(m_request.get()));

    PROPAGATE_ERROR(dispatcher.message("Started recording video at %dx%d, saving to '%s'\n",
        outputSize.width(), outputSize.height(), fileName.str().c_str()));

    m_recording = true;

    return true;
}

bool TaskVideoRecord::stopRecording()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Not initialized");

    if (!m_recording)
        ORIGINATE_ERROR("Recording had not been started, can't stop");

    Dispatcher &dispatcher = Dispatcher::getInstance();

    // stop the repeating request
    PROPAGATE_ERROR(dispatcher.stopRepeat());

    // stop recording
    PROPAGATE_ERROR(m_videoPipeline->stop());

    // Wait until all pending captures are done before destroying the stream
    PROPAGATE_ERROR(dispatcher.waitForIdle());

    if (m_videoStream)
    {
        // disable the output stream
        PROPAGATE_ERROR(dispatcher.disableOutputStream(m_request.get(), m_videoStream.get()));
        m_videoStream.reset();
    }

    // start the repeating request again to get the preview working
    PROPAGATE_ERROR(dispatcher.startRepeat(m_request.get()));

    if (m_videoPipeline)
    {
        // destroy the video pipeline
        PROPAGATE_ERROR(m_videoPipeline->destroy());
        delete m_videoPipeline;
        m_videoPipeline = NULL;
    }

    PROPAGATE_ERROR(dispatcher.message("Stopped recording video\n"));

    m_recording = false;

    return true;
}

bool TaskVideoRecord::toggleRecording()
{
    if (m_recording)
        PROPAGATE_ERROR(stopRecording());
    else
        PROPAGATE_ERROR(startRecording());
    return true;
}

bool TaskVideoRecord::shutdown()
{
    if (!m_initialized)
        return true;

    PROPAGATE_ERROR_CONTINUE(stop());

    PROPAGATE_ERROR_CONTINUE(m_perfTracker->shutdown());
    m_perfTracker.reset();

    Dispatcher &dispatcher = Dispatcher::getInstance();

    PROPAGATE_ERROR_CONTINUE(dispatcher.m_outputSize.unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskVideoRecord::restartStreams)));
    PROPAGATE_ERROR_CONTINUE(dispatcher.m_sensorModeValid.unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskVideoRecord::onSensorModeValidChanged)));
    PROPAGATE_ERROR_CONTINUE(dispatcher.m_deviceOpen.unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskVideoRecord::onDeviceOpenChanged)));
    PROPAGATE_ERROR_CONTINUE(dispatcher.m_captureYuvFormat.unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskVideoRecord::restartStreams)));

    m_initialized = false;

    return true;
}

}; // namespace ArgusSamples
