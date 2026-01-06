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

#ifndef TASK_VIDEO_RECORD_H
#define TASK_VIDEO_RECORD_H

#include <Argus/Argus.h>

#include "ITask.h"
#include "UniquePointer.h"
#include "IObserver.h"
#include "TrackedUniqueObject.h"

namespace ArgusSamples
{

class SessionPerfTracker;
class VideoPipeline;

/**
 * This task records a video and saves it to a file using gstreamer. It also creates a preview
 * stream and display it using the composer.
 */
class TaskVideoRecord : public ITask, public IObserver
{
public:
    TaskVideoRecord();
    virtual ~TaskVideoRecord();

    virtual bool initialize();
    virtual bool shutdown();

    virtual bool start();
    virtual bool stop();

    /**
     * Start recording
     */
    bool startRecording();
    /**
     * Stop recording
     */
    bool stopRecording();
    /**
     * Toggle recording
     */
    bool toggleRecording();

private:
    bool m_initialized;                 ///< set if initialized
    bool m_running;                     ///< set if preview is running
    bool m_wasRunning;                  ///< set if was running before the device had been closed
    bool m_prevRunning;                 ///< set if was running before the sensorModeValid is set to false
    bool m_recording;                   ///< if set recording is active
    uint32_t m_captureIndex;            ///< Incrementing capture index

    VideoPipeline *m_videoPipeline;     ///< video pipeline

    UniquePointer<SessionPerfTracker> m_perfTracker;

    TrackedUniqueObj<Argus::Request> m_request;             ///< Argus request
    Argus::UniqueObj<Argus::OutputStream> m_videoStream;    ///< Argus video output stream
    Argus::UniqueObj<Argus::OutputStream> m_previewStream;  ///< Argus preview output stream

    /**
     * Callback when the device is opened/closed.
     */
    bool onDeviceOpenChanged(const Observed &source);

    /**
     * Callback when the sensorModeValid is changed.
     */
    bool onSensorModeValidChanged(const Observed &source);

    /**
     * Restart when output size changes
     */
    bool restartStreams(const Observed &source);
};

}; // namespace ArgusSamples

#endif // TASK_VIDEO_RECORD_H
