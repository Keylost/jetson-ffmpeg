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

#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>
#include <unistd.h> // for useconds_t

#include "Ordered.h"

namespace ArgusSamples
{

/**
 * Base class for threads. Derived classes need to implement 'threadInitialize', 'threadExecute'
 * and 'threadShutdown'. This class handles the transition between the thread states.
 */
class Thread
{
public:
    Thread();
    virtual ~Thread();

    /**
     * Initialize
     */
    bool initialize();
    /**
     * Shutdown
     */
    bool shutdown();

    /**
     * Wait until the thread is in 'running' state
     *
     * @param timeout [in] timeout in us
     */
    bool waitRunning(useconds_t timeoutUs = 5 * 1000 * 1000);

 protected:
    virtual bool threadInitialize() = 0;
    virtual bool threadExecute() = 0;
    virtual bool threadShutdown() = 0;

    /**
     * Request thread shutdown
     */
    bool requestShutdown()
    {
        m_doShutdown = true;
        return true;
    }

    Ordered<bool> m_doShutdown; ///< set to request shutdown of the thread

private:
    pthread_t m_threadID;       ///< thread ID

    /**
     * Thread states
     */
    enum ThreadState
    {
        THREAD_INACTIVE,        ///< is inactive
        THREAD_INITIALIZING,    ///< is initializing
        THREAD_RUNNING,         ///< is running
        THREAD_FAILED,          ///< has failed
        THREAD_DONE,            ///< execution done
    };
    Ordered<ThreadState> m_threadState;

    bool threadFunction();

    static void *threadFunctionStub(void *dataPtr);
};

} // namespace ArgusSamples

#endif // THREAD_H
