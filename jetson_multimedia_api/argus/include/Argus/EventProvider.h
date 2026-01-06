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

/**
 * @file
 * <b>Libargus API: Event Provider API</b>
 *
 * @b Description: Defines the EventProvider interface.
 */

#ifndef _ARGUS_EVENT_PROVIDER_H
#define _ARGUS_EVENT_PROVIDER_H

namespace Argus
{

/**
 * @class IEventProvider
 *
 * Interface for an object which generates Events (such as CaptureSession).
 *
 * Any generated Events are initially stored by the provider itself, and they
 * are not copied out to public EventQueues until waitForEvents() is called.
 * If at any time there is an event type offered by a provider that is not
 * accepted by an active EventQueue created by that provider, all events of
 * that type will be discarded.
 *
 * @ingroup ArgusCaptureSession
 */
DEFINE_UUID(InterfaceID, IID_EVENT_PROVIDER, 523ed330,25dc,11e5,867f,08,00,20,0c,9a,66);
class IEventProvider : public Interface
{
public:
    static const InterfaceID& id() { return IID_EVENT_PROVIDER; }

    /**
     * Returns a list of event types that this provider can generate.
     * @param[out] types A vector that will be populated by the available event types.
     *
     * @returns success/status of the call.
     */
    virtual Status getAvailableEventTypes(std::vector<EventType>* types) const = 0;

    /**
     * Creates an event queue for events of the given type(s)
     * @param[in] eventTypes The list of event types for the queue.
     * @param[out] status An optional pointer to return success/status.
     *
     * @returns the new EventQueue object, or NULL on failure.
     */
    virtual EventQueue* createEventQueue(const std::vector<EventType>& eventTypes,
                                         Status* status = NULL) = 0;

    /**
     * Waits for and transfers any pending events from the provider to the
     * provided queues.
     *
     * Ownership of all events transfered to a queue will be passed from the
     * provider to the queue, and these event object pointers will remain
     * valid until the queue is destroyed or until the next call to this
     * function with that queue. In other words, any events in a queue will be
     * destroyed when the queue is provided to another call of this function,
     * regardless of whether or not it receives any new events, or when the
     * queue is destroyed.
     *
     * If more than one given queue accepts events of the same type, only the
     * first of these queues will receive events of that type.
     *
     * Any events that are not copied to queues by this function are left in
     * the provider until they are queried using a queue receiving events of
     * that type.
     *
     * If there are no pending events of the requested types at the time this
     * function is called, it will block until one is available or a timeout
     * occurs.
     *
     * @param[in] queues The list of queues to transfer events to.
     * @param[in] timeout The maximum time (in nanoseconds) to wait for new events.
     *
     * @returns success/status of the call.
     */
    virtual Status waitForEvents(const std::vector<EventQueue*>& queues,
                                 uint64_t timeout = TIMEOUT_INFINITE) = 0;

    /**
     * Variant of waitForEvents() that waits for only one EventQueue.
     */
    virtual Status waitForEvents(EventQueue* queue,
                                 uint64_t timeout = TIMEOUT_INFINITE) = 0;

protected:
    ~IEventProvider() {}
};

} // namespace Argus

#endif // _ARGUS_EVENT_PROVIDER_H
