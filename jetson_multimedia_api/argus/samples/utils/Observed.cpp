/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
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

#include "Observed.h"

#include "Error.h"

namespace ArgusSamples
{

Observed::Observed()
{
}

Observed::~Observed()
{
}

bool Observed::registerObserver(IObserver *observer, IObserver::CallbackFunction callback)
{
    // iterate through the registered observers, check if the observer/callback function already
    // exists
    for (std::list<Registered>::const_iterator it = m_observers.begin();
         it != m_observers.end(); ++it)
    {
        const Registered &registered = *it;
        if ((registered.m_observer == observer) && (registered.m_callback == callback))
        {
            ORIGINATE_ERROR("Observer with 'callback' already registered");
        }
    }

    // add to the observer list
    m_observers.push_front(Registered(observer, callback));

    // initial callback to feed with current value
    PROPAGATE_ERROR((observer->*callback)(*this));

    return true;
}

bool Observed::unregisterObserver(IObserver *observer, IObserver::CallbackFunction callback)
{
    // iterate through the registered observers, search for the observer/callback combination and
    // erase
    for (std::list<Registered>::iterator it = m_observers.begin();
         it != m_observers.end(); ++it)
    {
        const Registered &registered = *it;
        if ((registered.m_observer == observer) && (registered.m_callback == callback))
        {
            m_observers.erase(it);
            return true;
        }
    }

    ORIGINATE_ERROR("Observer and callback not found");
}

bool Observed::notifyObservers() const
{
    // iterate through the registered observers, and trigger callback on observers
    for (std::list<Registered>::const_iterator it = m_observers.begin();
         it != m_observers.end(); ++it)
    {
        const Registered &registered = *it;
        PROPAGATE_ERROR(((registered.m_observer)->*(registered.m_callback))(*this));
    }

    return true;
}

}; // namespace ArgusSamples
