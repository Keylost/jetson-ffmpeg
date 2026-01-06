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

#ifndef OBSERVED_H
#define OBSERVED_H

#include <list>

#include "IObserver.h"

namespace ArgusSamples
{

/**
 * Interface for observed classes. Observers register to observed objects and get called
 * through a callback function.
 */
class Observed
{
public:
    Observed();

    virtual ~Observed();

    /**
     * Register an observer notified when the value has changed. It's an error to register the
     * same observer/callback combination twice.
     *
     * @param observer [in] The observer to register
     * @param callback [in] The callback to register
     */
    bool registerObserver(IObserver *observer, IObserver::CallbackFunction callback);

    /**
     * Unregister an observer
     *
     * @param observer [in] The observer to unregister
     * @param callback [in] The callback to unregister
     */
    bool unregisterObserver(IObserver *observer, IObserver::CallbackFunction callback);

protected:
    bool notifyObservers() const;

private:
    struct Registered
    {
        Registered(IObserver *observer, IObserver::CallbackFunction callback)
            : m_observer(observer)
            , m_callback(callback)
        {
        }
        IObserver *m_observer;
        IObserver::CallbackFunction m_callback;
    };
    std::list<Registered> m_observers;
};

}; // namespace ArgusSamples

#endif // OBSERVED_H
