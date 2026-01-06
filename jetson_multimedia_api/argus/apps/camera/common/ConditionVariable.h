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

#ifndef CAMERA_COMMON_CONDITION_VARIABLE_H
#define CAMERA_COMMON_CONDITION_VARIABLE_H

#include <pthread.h>

namespace ArgusSamples
{

class Mutex;

/**
 * Conditional
 */
class ConditionVariable
{
public:
    ConditionVariable();
    ~ConditionVariable();

    /**
     * Create the underlying condition variable. This method must be called before any other
     * methods.
     */
    bool initialize();

    /**
     * Destroy the underlying condition variable. After this call, this object can no longer be used
     * (until and unless a future call to @c initialize()). Calling this method if the
     * object is not initialized generates no error, but silently returns.
     */
    bool shutdown();

    /**
     * Broadcast the condition variable.  This method is declared @c const for convenience.
     */
    bool broadcast() const;

    /**
     * Signal the condition variable.  This method is declared @c const for convenience.
     */
    bool signal() const;

    /**
     * Wait on the condition variable.  This method is declared @c const for convenience.
     * @param [in] mutex The mutex that will be released while waiting. When multiple threads
     *                   are waiting concurrently, they must all be using the same mutex.
     */
    bool wait(const Mutex& mutex) const;

private:
    bool m_initialized;
    /**
     * pthread conditional variable, this is 'mutable' so that 'const' functions can be used.
     */
    mutable pthread_cond_t m_cond;

    /**
     * Hide copy constructor and assignment operator
     */
    ConditionVariable(ConditionVariable &other);
    const ConditionVariable& operator = (const ConditionVariable&);
};

} // namespace ArgusSamples

#endif // CAMERA_COMMON_CONDITION_VARIABLE_H
