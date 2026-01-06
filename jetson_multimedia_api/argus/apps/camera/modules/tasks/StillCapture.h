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

#ifndef TASK_STILL_CAPTURE_H
#define TASK_STILL_CAPTURE_H

#include <Argus/Argus.h>
#include <EGLStream/EGLStream.h>

#include "ITask.h"
#include "UniquePointer.h"
#include "IObserver.h"
#include "TrackedUniqueObject.h"

namespace ArgusSamples
{

class SessionPerfTracker;

/**
 * This task captures still images
 */
class TaskStillCapture : public ITask, public IObserver
{
public:
    TaskStillCapture();
    virtual ~TaskStillCapture();

    virtual bool initialize();
    virtual bool shutdown();

    virtual bool start();
    virtual bool stop();

    /**
     * Capture one image
     */
    bool execute();

private:
    bool m_initialized;                 ///< set if initialized
    bool m_running;                     ///< set if preview is running
    bool m_wasRunning;                  ///< set if was running before the device had been closed
    bool m_prevRunning;                 ///< set if was running before the sensorModeValid is set to false
    uint32_t m_captureIndex;            ///< Incrementing capture index

    UniquePointer<SessionPerfTracker> m_perfTracker;

    TrackedUniqueObj<Argus::Request> m_previewRequest;      ///< Argus preview request
    Argus::UniqueObj<Argus::OutputStream> m_previewStream;  ///< Argus preview stream

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

#endif // TASK_STILL_CAPTURE_H
