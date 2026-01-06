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
 * <b>Libargus API: Event Queue API</b>
 *
 * @b Description: Defines the EventQueue object and interface.
 */

#ifndef _ARGUS_EVENT_QUEUE_H
#define _ARGUS_EVENT_QUEUE_H

namespace Argus
{

/**
 * Object to receive and expose Events from an IEventProvider.
 *
 * @see IEventProvider::createEventQueue.
 *
 * @defgroup ArgusEventQueue EventQueue
 * @ingroup ArgusObjects
 */
class EventQueue : public InterfaceProvider, public Destructable
{
protected:
    ~EventQueue() {}
};

/**
 * @class IEventQueue
 *
 * Interface to the core EventQueue methods.
 *
 * @ingroup ArgusEventQueue
 */
DEFINE_UUID(InterfaceID, IID_EVENT_QUEUE, 944b11f6,e512,49ad,8573,fc,82,3e,02,25,ed);
class IEventQueue : public Interface
{
public:
    static const InterfaceID& id() { return IID_EVENT_QUEUE; }

    /**
     * Returns the event types that this queue will receive.
     * @param[out] types This vector will be populated with the event types
     *                   registered to this queue.
     *
     * @returns success/status of the call.
     */
    virtual Status getEventTypes(std::vector<EventType>* types) const = 0;

    /**
     * Returns the next event in the queue (that is, the event at index 0). The returned
     * event will be removed from the queue, though the object will remain valid
     * according to the rules described by waitForEvents().
     * If the queue is empty, returns NULL.
     */
    virtual const Event* getNextEvent() = 0;

    /**
     * Returns the number of events in the queue.
     */
    virtual uint32_t getSize() const = 0;

    /**
     * Returns the event with the given index, where index 0 corresponds to the oldest
     * event and [getSize() - 1] is the newest. The returned event is not removed
     * from the queue. If index is not in [0, getSize()-1], NULL is returned.
     */
    virtual const Event* getEvent(uint32_t index) const = 0;

protected:
    ~IEventQueue() {}
};

} // namespace Argus

#endif // _ARGUS_EVENT_QUEUE_H
