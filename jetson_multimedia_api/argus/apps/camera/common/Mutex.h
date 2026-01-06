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

#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>

#include "Error.h"

namespace ArgusSamples
{

class ConditionVariable;

/**
 * Mutex
 */
class Mutex
{
public:
    Mutex();
    ~Mutex();

    /**
     * Create the underlying mutex. This method must be called before any other methods.
     */
    bool initialize();

    /**
     * Destroy the underlying mutex. After this call, this object can no longer be used
     * (until and unless a future call to @c initialize()). Calling this method if the
     * object is not initialized generates no error, but silently returns.
     */
    bool shutdown();

    /**
     * Lock the mutex. This method is declared @c const for convenience.
     */
    bool lock() const;

    /**
     * Unlock the mutex. This method is declared @c const for convenience.
     */
    bool unlock() const;

private:
    bool m_initialized;
    /**
     * pthread mutex, this is 'mutable' so that 'const' functions can be used.
     */
    mutable pthread_mutex_t m_mutex;

    /**
     * Hide copy constructor and assignment operator
     */
    Mutex(Mutex &other);
    const Mutex& operator = (const Mutex&);

    friend class ConditionVariable;

    pthread_mutex_t* getPThreadMutex() const
    {
        return &m_mutex;
    }
};

/**
 * An RAII-style class for acquiring a Mutex.
 * The mutex is acquired in the constructor and released in the destructor.
 * This class is NOT to be subclassed.
 */
class ScopedMutex
{
public:
    explicit ScopedMutex(Mutex& mutex)
        : m_mutex(&mutex)
        , m_isLocked(false)
    {
        m_isLocked = m_mutex->lock();
    }

    ~ScopedMutex()
    {
        if (m_isLocked)
            m_mutex->unlock();
    }

    bool expectLocked() const
    {
        if (!m_isLocked)
            ORIGINATE_ERROR("Expected mutex to be locked");
        return true;
    }

private:
    Mutex *m_mutex;
    bool m_isLocked;

    /**
     * Hide default/copy constructor and assignment operator
     */
    ScopedMutex();
    ScopedMutex(ScopedMutex &other);
    ScopedMutex& operator = (const ScopedMutex&);
};

} // namespace ArgusSamples

#endif // MUTEX_H
