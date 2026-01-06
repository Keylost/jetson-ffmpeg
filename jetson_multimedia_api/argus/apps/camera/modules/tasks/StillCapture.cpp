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


#include <sstream>
#include <iomanip>
#include <unistd.h>

#include "StillCapture.h"
#include "Composer.h"
#include "Dispatcher.h"
#include "Error.h"
#include "PerfTracker.h"

namespace ArgusSamples
{

TaskStillCapture::TaskStillCapture()
    : m_initialized(false)
    , m_running(false)
    , m_wasRunning(false)
    , m_prevRunning(false)
    , m_captureIndex(0)
{
}

TaskStillCapture::~TaskStillCapture()
{
    shutdown();
}

bool TaskStillCapture::initialize()
{
    if (m_initialized)
        return true;

    Dispatcher &dispatcher = Dispatcher::getInstance();

    PROPAGATE_ERROR(dispatcher.m_deviceOpen.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskStillCapture::onDeviceOpenChanged)));
    PROPAGATE_ERROR(dispatcher.m_sensorModeValid.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskStillCapture::onSensorModeValidChanged)));
    PROPAGATE_ERROR(dispatcher.m_outputSize.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskStillCapture::restartStreams)));
    PROPAGATE_ERROR(dispatcher.m_captureYuvFormat.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskStillCapture::restartStreams)));


    m_perfTracker.reset(new SessionPerfTracker());
    if (!m_perfTracker)
        ORIGINATE_ERROR("Out of memory");

    m_initialized = true;

    return true;
}

bool TaskStillCapture::restartStreams(__attribute__((unused)) const Observed &source)
{
    if (m_running)
    {
        PROPAGATE_ERROR(stop());
        PROPAGATE_ERROR(start());
    }
    return true;
}

bool TaskStillCapture::onDeviceOpenChanged(const Observed &source)
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

bool TaskStillCapture::onSensorModeValidChanged(const Observed &source)
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

bool TaskStillCapture::start()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Not initialized");

    if (m_running)
        return true;

    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_TASK_START));

    Dispatcher &dispatcher = Dispatcher::getInstance();
    Composer &composer = Composer::getInstance();

    PROPAGATE_ERROR(dispatcher.createRequest(m_previewRequest, Argus::CAPTURE_INTENT_PREVIEW));

    // Create the preview stream
    PROPAGATE_ERROR(dispatcher.createOutputStream(m_previewRequest.get(), false, m_previewStream));

    Argus::IEGLOutputStream *iEGLOutputStream =
        Argus::interface_cast<Argus::IEGLOutputStream>(m_previewStream);
    if (!iEGLOutputStream)
        ORIGINATE_ERROR("Failed to get IEGLOutputStream interface");

    // render the preview stream
    PROPAGATE_ERROR(composer.bindStream(iEGLOutputStream->getEGLStream()));

    const Argus::Size2D<uint32_t> streamSize = iEGLOutputStream->getResolution();
    PROPAGATE_ERROR(composer.setStreamAspectRatio(iEGLOutputStream->getEGLStream(),
        (float)streamSize.width() / (float)streamSize.height()));
    PROPAGATE_ERROR(composer.setStreamActive(iEGLOutputStream->getEGLStream(), true));

    // Enable the preview stream
    PROPAGATE_ERROR(dispatcher.enableOutputStream(m_previewRequest.get(), m_previewStream.get()));

    // start the repeating request for the preview
    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_ISSUE_CAPTURE));
    PROPAGATE_ERROR(dispatcher.startRepeat(m_previewRequest.get()));

    m_running = true;

    return true;
}

bool TaskStillCapture::stop()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Not initialized");

    if (!m_running)
        return true;

    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_CLOSE_REQUESTED));

    Dispatcher &dispatcher = Dispatcher::getInstance();

    // stop the repeating request
    PROPAGATE_ERROR(dispatcher.stopRepeat());

    PROPAGATE_ERROR(dispatcher.waitForIdle());
    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_FLUSH_DONE));

    // disable the output stream
    PROPAGATE_ERROR(dispatcher.disableOutputStream(m_previewRequest.get(), m_previewStream.get()));

    Argus::IEGLOutputStream *iEGLOutputStream =
        Argus::interface_cast<Argus::IEGLOutputStream>(m_previewStream);
    if (!iEGLOutputStream)
        ORIGINATE_ERROR("Failed to get IEGLOutputStream interface");

    // disconnect the EGL stream
    iEGLOutputStream->disconnect();

    // unbind the preview stream from the composer
    PROPAGATE_ERROR(Composer::getInstance().unbindStream(iEGLOutputStream->getEGLStream()));

    // destroy the preview stream
    m_previewStream.reset();

    // destroy the preview request
    PROPAGATE_ERROR(m_previewRequest.reset());

    PROPAGATE_ERROR(m_perfTracker->onEvent(SESSION_EVENT_CLOSE_DONE));

    m_running = false;

    return true;
}

bool TaskStillCapture::execute()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Not initialized");
    if (!m_running)
        ORIGINATE_ERROR("Not running");

    Dispatcher &dispatcher = Dispatcher::getInstance();

    TrackedUniqueObj<Argus::Request> stillRequest;
    PROPAGATE_ERROR(dispatcher.createRequest(stillRequest, Argus::CAPTURE_INTENT_STILL_CAPTURE));

    // Create the still stream
    Argus::UniqueObj<Argus::OutputStream> stillStream;
    PROPAGATE_ERROR(dispatcher.createOutputStream(stillRequest.get(), true, stillStream));

    // Enable the still stream
    PROPAGATE_ERROR(dispatcher.enableOutputStream(stillRequest.get(), stillStream.get()));

    // Create the frame consumer
    Argus::UniqueObj<EGLStream::FrameConsumer> consumer(
        EGLStream::FrameConsumer::create(stillStream.get()));
    EGLStream::IFrameConsumer *iFrameConsumer =
        Argus::interface_cast<EGLStream::IFrameConsumer>(consumer);
    if (!iFrameConsumer)
        ORIGINATE_ERROR("Failed to create FrameConsumer");

    // do the capture
    PROPAGATE_ERROR(dispatcher.capture(stillRequest.get()));

    // aquire the frame
    Argus::UniqueObj<EGLStream::Frame> frame(iFrameConsumer->acquireFrame());
    if (!frame)
        ORIGINATE_ERROR("Failed to aquire frame");

    // Use the IFrame interface to provide access to the Image in the Frame.
    EGLStream::IFrame *iFrame = Argus::interface_cast<EGLStream::IFrame>(frame);
    if (!iFrame)
        ORIGINATE_ERROR("Failed to get IFrame interface.");

    EGLStream::Image *image = iFrame->getImage();
    if (!image)
        ORIGINATE_ERROR("Failed to get image.");

    switch (dispatcher.m_stillFileType.get())
    {
        case STILL_FILE_TYPE_JPG:
        {
            // Get the JPEG interface.
            EGLStream::IImageJPEG *iJPEG =
                Argus::interface_cast<EGLStream::IImageJPEG>(image);
            if (!iJPEG)
                ORIGINATE_ERROR("Failed to get IImageJPEG interface.");

            // build the file name
            std::ostringstream fileName;
            fileName << dispatcher.m_outputPath.get();
            if (dispatcher.m_outputPath.get() != "/dev/null")
                fileName << "/image" << std::setfill('0') << std::setw(4) <<
                m_captureIndex << ".jpg";

            PROPAGATE_ERROR(validateOutputPath(fileName.str().c_str()));

            // Write a JPEG to disk.
            if (iJPEG->writeJPEG(fileName.str().c_str()) == Argus::STATUS_OK)
            {
                PROPAGATE_ERROR(dispatcher.message("Captured a still image to '%s'\n",
                                                   fileName.str().c_str()));
            }
            else
            {
                ORIGINATE_ERROR("Failed to write JPEG to '%s'\n", fileName.str().c_str());
            }
        }
        break;

        case STILL_FILE_TYPE_HEADERLESS:
        {
            // Get the HEADERLESS_FILE interface.
            EGLStream::IImageHeaderlessFile *iHeaderlessFile =
                Argus::interface_cast<EGLStream::IImageHeaderlessFile>(image);
            if (!iHeaderlessFile)
                ORIGINATE_ERROR("Failed to get IImageHeaderlessFile interface.");

            EGLStream::IImage2D *i2D =
                Argus::interface_cast<EGLStream::IImage2D>(image);
            if (!i2D)
                ORIGINATE_ERROR("Failed to get IImage2D interface.");
            const Argus::Size2D<uint32_t> size = i2D->getSize();

            // build the file name
            std::ostringstream fileName;
            fileName << dispatcher.m_outputPath.get();
            if (dispatcher.m_outputPath.get() != "/dev/null")
            {
                fileName << "/image_" <<
                    size.width() << "x" << size.height() << "_" <<
                    std::setfill('0') << std::setw(4) << m_captureIndex <<
                    "." << dispatcher.m_captureYuvFormat.toString();
            }

            PROPAGATE_ERROR(validateOutputPath(fileName.str().c_str()));

            // Write a headerless, unencoded image to disk.
            if (iHeaderlessFile->writeHeaderlessFile(fileName.str().c_str()) == Argus::STATUS_OK)
            {
                PROPAGATE_ERROR(dispatcher.message("Captured a still image to '%s'\n",
                                                   fileName.str().c_str()));
            }
            else
            {
                ORIGINATE_ERROR("Failed to write headerless raw image to '%s'\n",
                                fileName.str().c_str());
            }
        }
        break;

        default:
            ORIGINATE_ERROR("unknown still image file type");
    }

    ++m_captureIndex;

    // release the frame.
    frame.reset();

    // destroy the still stream
    PROPAGATE_ERROR(dispatcher.disableOutputStream(stillRequest.get(), stillStream.get()));
    stillStream.reset();

    // destroy the still request
    PROPAGATE_ERROR(stillRequest.reset());

    // destroy the still consumer
    consumer.reset();

    return true;
}

bool TaskStillCapture::shutdown()
{
    if (!m_initialized)
        return true;

    // stop the module
    PROPAGATE_ERROR_CONTINUE(stop());

    PROPAGATE_ERROR_CONTINUE(m_perfTracker->shutdown());
    m_perfTracker.reset();

    Dispatcher &dispatcher = Dispatcher::getInstance();

    PROPAGATE_ERROR_CONTINUE(dispatcher.m_outputSize.unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskStillCapture::restartStreams)));
    PROPAGATE_ERROR_CONTINUE(dispatcher.m_sensorModeValid.unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskStillCapture::onSensorModeValidChanged)));
    PROPAGATE_ERROR_CONTINUE(dispatcher.m_deviceOpen.unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskStillCapture::onDeviceOpenChanged)));
    PROPAGATE_ERROR_CONTINUE(dispatcher.m_captureYuvFormat.unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskStillCapture::restartStreams)));

    m_initialized = false;

    return true;
}

}; // namespace ArgusSamples
