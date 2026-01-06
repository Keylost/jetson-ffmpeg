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

#ifndef CAMERA_MODULES_INITONCE_H
#define CAMERA_MODULES_INITONCE_H

#include <unistd.h> // for usleep()
#include <assert.h>

#include "Ordered.h"

namespace ArgusSamples
{

/**
 * This class supports one-time initialization. Example usage:
 *
 * void func()
 * {
 *     static InitOnce initOnce;
 *
 *     if (initOnce.begin())
 *     {
 *         if (doInitialization())
 *             initOnce.complete();
 *         else
 *             initOnce.failed();
 *     }
 * }
 *
 */
class InitOnce
{
private:
    /// state values
    enum State
    {
        STATE_INIT = 0,         ///< default state
        STATE_BEGIN = 1,        ///< one thread began one-time initialization
        STATE_COMPLETE = 2,     ///< one-time initialization
    };

    Ordered<State> m_state;   ///< Initialization state

public:
    /**
     * Constructor
     */
    InitOnce() :
        m_state(STATE_INIT)
    {
    }

    /**
     * Call at the begin of the initialization code.
     *
     * @return true if initialization needs to be done, false if it already had been done
     */
    bool begin()
    {
        while (m_state != STATE_COMPLETE)
        {
            if (m_state.compareExchange(STATE_INIT, STATE_BEGIN))
                return true;

            // wait and check again
            usleep(100);
        }

        return false;
    }

    /**
     * Call at the end of the initialization code.
     */
    void complete()
    {
        assert(m_state == STATE_BEGIN);
        m_state = STATE_COMPLETE;
    }

    /**
     * Call if initialization code failed.
     */
    void failed()
    {
        assert(m_state == STATE_BEGIN);
        m_state = STATE_INIT;
    }
};

} // namespace ArgusSamples

#endif // CAMERA_MODULES_INITONCE_H
