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

#ifndef TASK_MULTI_EXPOSURE_H
#define TASK_MULTI_EXPOSURE_H

#include <list>

#include <Argus/Argus.h>

#include "ITask.h"
#include "Value.h"
#include "IObserver.h"
#include "TrackedUniqueObject.h"

namespace ArgusSamples
{

/**
 * This task captures multiple streams with different exposure compensation values.
 */
class TaskMultiExposure : public ITask, public IObserver
{
public:
    TaskMultiExposure();
    virtual ~TaskMultiExposure();

    /** @name ITask methods */
    /**@{*/
    virtual bool initialize();
    virtual bool shutdown();
    virtual bool start();
    virtual bool stop();
    /**@}*/

private:
    // the range values need to be initialized first, some Value<> members below use them
    // for the validator
    Value<Argus::Range<uint32_t> > m_exposureStepsRange;    ///< allowed exposure steps

public:
    Value<uint32_t> m_exposureSteps;    ///< steps within the exposure range
    Value<Argus::Range<float> > m_exposureRange;  ///< in eV, e.g. -1,2 results in exposures from
                                        /// -1 eV to +2 eV

private:
    bool m_initialized;                 ///< set if initialized
    bool m_running;                     ///< set if preview is running
    bool m_wasRunning;                  ///< set if was running before the device had been closed
    bool m_prevRunning;                 ///< set if was running before the sensorModeValid is set to false

    /**
     * For each exposure level there is one request where the exposure compensation is set to the
     * correct value. Each request outputs to a stream which is rendered.
     */
    class ExpLevel
    {
    public:
        ExpLevel();
        ~ExpLevel();

        bool shutdown();
        bool initialize(float exposureCompensation);

        TrackedUniqueObj<Argus::Request> m_request; ///< Argus request
        Argus::UniqueObj<Argus::OutputStream> m_outputStream; ///< Argus output stream
    };

    std::list<ExpLevel*> m_expLevels;   ///< exposure level

    /**
     * Callback when the device is opened/closed.
     */
    bool onDeviceOpenChanged(const Observed &source);

    /**
     * Callback when the sensorModeValid is changed.
     */
    bool onSensorModeValidChanged(const Observed &source);

    /**
     * Callback when the exposure range or steps changes.
     */
    bool onParametersChanged(const Observed &source);

    /**
     * Shut down the exposure level streams.
     */
    bool shutdownExpLevels();

    /**
     * Restart when output size changes
     */
    bool restartStreams(const Observed &source);
};

}; // namespace ArgusSamples

#endif // TASK_MULTI_EXPOSURE_H
