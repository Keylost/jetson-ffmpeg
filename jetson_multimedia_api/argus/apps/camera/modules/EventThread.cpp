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

#include "EventThread.h"
#include "Dispatcher.h"
#include "Util.h"
#include "PerfTracker.h"

#include <Argus/Ext/InternalFrameCount.h>

namespace ArgusSamples {

EventThread::EventThread(Argus::CaptureSession *session,
                         SessionPerfTracker *sessionPerfTracker)
    : m_session(session)
    , m_sessionPerfTracker(sessionPerfTracker)
{
}

EventThread::~EventThread()
{
}

bool EventThread::threadInitialize()
{
    std::vector<Argus::EventType> eventTypes;
    eventTypes.push_back(Argus::EVENT_TYPE_CAPTURE_COMPLETE);

    PROPAGATE_ERROR(Dispatcher::getInstance().createEventQueue(eventTypes, m_eventQueue,
        m_session));

    return true;
}

bool EventThread::threadExecute()
{
    Dispatcher &dispatcher = Dispatcher::getInstance();

    // wait for events (use a time out to allow the thread to be shutdown even if there are no
    // new events)
    PROPAGATE_ERROR(dispatcher.waitForEvents(m_eventQueue.get(), TimeValue::fromMSec(100),
        m_session));

    Argus::IEventQueue *iEventQueue =
        Argus::interface_cast<Argus::IEventQueue>(m_eventQueue.get());
    if (!iEventQueue)
        ORIGINATE_ERROR("Failed to get iEventQueue");

    for (uint32_t i = 0; i < iEventQueue->getSize(); i++)
    {
        const Argus::Event *event = iEventQueue->getEvent(i);
        const Argus::IEvent *iEvent = Argus::interface_cast<const Argus::IEvent>(event);
        if (!iEvent)
            ORIGINATE_ERROR("Failed to get IEvent interface");

        if (iEvent->getEventType() == Argus::EVENT_TYPE_CAPTURE_COMPLETE)
        {
            PROPAGATE_ERROR(m_sessionPerfTracker->onEvent(SESSION_EVENT_REQUEST_RECEIVED));

            const Argus::IEventCaptureComplete *iEventCaptureComplete
                 = Argus::interface_cast<const Argus::IEventCaptureComplete>(event);
            const Argus::CaptureMetadata *metaData = iEventCaptureComplete->getMetadata();
            if (metaData)
            {
                const Argus::ICaptureMetadata *iCaptureMeta =
                    Argus::interface_cast<const Argus::ICaptureMetadata>(metaData);
                if (iCaptureMeta)
                {
                    /// @todo IEvent documentation says the time value is in nano seconds, but
                    ///       actually it's in micro seconds.
                    const TimeValue latency =
                        TimeValue::fromUSec(iEvent->getTime()) -
                        TimeValue::fromNSec(iCaptureMeta->getSensorTimestamp());
                    PROPAGATE_ERROR(m_sessionPerfTracker->onEvent(
                        SESSION_EVENT_REQUEST_LATENCY, latency.toUSec()));

                    const TimeValue sensorTime =
                        TimeValue::fromNSec(iCaptureMeta->getSensorTimestamp());
                    PROPAGATE_ERROR(m_sessionPerfTracker->onEvent(
                        SESSION_EVENT_FRAME_PERIOD, sensorTime.toUSec()));

                    // AF
                    std::vector< Argus::AcRegion > regions;
                    std::vector<float> sharpnessScore;
                    if (iCaptureMeta->getAfRegions(&regions) != Argus::STATUS_OK)
                        ORIGINATE_ERROR("Failed to get AF regions");

                    if (iCaptureMeta->getSharpnessScore(&sharpnessScore) != Argus::STATUS_OK)
                        ORIGINATE_ERROR("Failed to get sharpness score");

                    PROPAGATE_ERROR(dispatcher.message("Focus control info: focuser position %d ",
                                    iCaptureMeta->getFocuserPosition()));
                    for (uint32_t j = 0; j < regions.size(); j++)
                    {
                        PROPAGATE_ERROR(dispatcher.message(" region %d %d %d %d, score %f   ",
                                    regions[j].left(), regions[j].top(), regions[j].right(),
                                    regions[j].bottom(), sharpnessScore[j]));
                    }
                    PROPAGATE_ERROR(dispatcher.message("\n"));

                    // bayerHistogram
                    Argus::Rectangle<uint32_t> region;
                    region = iCaptureMeta->getBayerHistogramRegion();
                    PROPAGATE_ERROR(dispatcher.message("BayerHistogram region %d %d %d %d, \n",
                                region.left(), region.top(), region.right(), region.bottom()));

                    // Flicker
                    Argus::AeFlickerState state = iCaptureMeta->getFlickerState();
                    PROPAGATE_ERROR(dispatcher.message("Flicker state %s \n", state.getName()));

                    PROPAGATE_ERROR(dispatcher.message("aperture info: aperture position %d \n",
                    iCaptureMeta->getAperturePosition()));
                }

                const Argus::Ext::IInternalFrameCount *iInternalFrameCount =
                    Argus::interface_cast<const Argus::Ext::IInternalFrameCount>(metaData);
                if (iInternalFrameCount)
                {
                    const uint64_t currentFrameCount = iInternalFrameCount->getInternalFrameCount();
                    PROPAGATE_ERROR(m_sessionPerfTracker->onEvent(SESSION_EVENT_FRAME_COUNT,
                        currentFrameCount));
                }
            }
        }
    }

    return true;
}

bool EventThread::threadShutdown()
{
    return true;
}

}; // namespace ArgusSamples
