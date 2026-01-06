/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION. All rights reserved.
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

#ifndef PERFTRACKER_H
#define PERFTRACKER_H

#include <stddef.h>

#include "Util.h" // for TimeValue
#include "Ordered.h"
#include "UniquePointer.h"

namespace Argus { class CaptureSession; }

namespace ArgusSamples
{

class EventThread;

/**
 * Global events
 */
enum GlobalEvent
{
    GLOBAL_EVENT_APP_START,
    GLOBAL_EVENT_APP_INITIALIZED,
    GLOBAL_EVENT_DISPLAY
};

/**
 * Used to track global performance events.
 */
class PerfTracker
{
public:
    /**
     * Get the window instance.
     */
    static PerfTracker &getInstance();

    /**
     * Trigger a global event.
     *
     * @param type [in] event type
     */
    bool onEvent(GlobalEvent event);

    /**
     * @returns the point in time when the app had been started
     */
    const TimeValue& appStartTime() const
    {
        return m_appStartTime;
    }

    /**
     * @returns the point in time when app initialization finished
     */
    const TimeValue& appInitializedTime() const
    {
        return m_appInitializedTime;
    }

    /**
     * @returns a new session Id
     */
    uint32_t getNewSessionID()
    {
        return ++m_sessionId;
    }

private:
    PerfTracker();
    ~PerfTracker();

    // this is a singleton, hide copy constructor etc.
    PerfTracker(const PerfTracker&);
    PerfTracker& operator=(const PerfTracker&);

    bool initialize();

    // global performance values
    TimeValue m_appStartTime;
    TimeValue m_appInitializedTime;

    TimeValue m_firstDisplayTime;   //< time at which the first display event had been received
    uint64_t m_displayCount;        //< display events since first display time

    Ordered<uint32_t> m_sessionId;
};

/**
 * Session events
 */
enum SessionEvent
{
    SESSION_EVENT_TASK_START,
    SESSION_EVENT_ISSUE_CAPTURE,
    SESSION_EVENT_REQUEST_RECEIVED,
    SESSION_EVENT_REQUEST_LATENCY,
    SESSION_EVENT_FRAME_PERIOD,
    SESSION_EVENT_FRAME_COUNT,
    SESSION_EVENT_CLOSE_REQUESTED,
    SESSION_EVENT_FLUSH_DONE,
    SESSION_EVENT_CLOSE_DONE
};

/**
 * Used to track session performance events.
 */
class SessionPerfTracker
{
public:
    SessionPerfTracker();
    ~SessionPerfTracker();

    /**
     * Shutdown the session performance tracker.
     */
    bool shutdown();

    /**
     * Set the capture session to track. If not set the internal dispatcher session is tracked.
     *
     * @param session [in] capture session
     */
    bool setSession(Argus::CaptureSession *session);

    /**
     * Trigger a session event.
     *
     * @param type [in] event type
     * @param type [in] event type
     */
    bool onEvent(SessionEvent event, uint64_t value = 0);

private:
    uint32_t m_id;

    Argus::CaptureSession *m_session;
    UniquePointer<EventThread> m_eventThread;

    TimeValue m_taskStartTime;
    TimeValue m_issueCaptureTime;
    TimeValue m_requestReceivedTime;

    TimeValue m_firstRequestReceivedTime;
    uint64_t m_numberframesReceived;

    TimeValue m_closeRequestedTime;
    TimeValue m_flushDoneTime;
    TimeValue m_closeDoneTime;

    uint64_t m_lastFrameCount;
    int64_t m_totalFrameDrop;

    uint64_t m_minLatency;
    uint64_t m_maxLatency;
    uint64_t m_sumLatency;
    uint64_t m_countLatency;

    uint64_t m_previousSensorTime;
    uint64_t m_minFramePeriod;
    uint64_t m_maxFramePeriod;
    uint64_t m_sumFramePeriod;
    int64_t m_countFramePeriod;

    uint64_t m_statsMinLatency;
    uint64_t m_statsMaxLatency;
    uint64_t m_statsSumLatency;
    uint64_t m_statsCountLatency;
    uint64_t m_statsMinFramePeriod;
    uint64_t m_statsMaxFramePeriod;
    uint64_t m_statsSumFramePeriod;
    uint64_t m_statsCountFramePeriod;
    uint32_t m_statsFrameDropCount;
    uint32_t m_statsOutOfOrderCount;

    bool m_previousKpi;
};


}; // namespace ArgusSamples

#endif //PERFTRACKER_H

