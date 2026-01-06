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

#ifndef SCOPED_GUARD_H
#define SCOPED_GUARD_H

namespace ArgusSamples
{

/**
 * RAII-syle class performing an action when control flow leaves the scope. Typically the action
 * to be performed is to undo a previous action in case of an error.
 */
template <typename T> class ScopedGuard
{
public:
    /**
     * Action function type.
     */
    typedef bool (T::*ActionType)();

    /**
     * Constructor
     *
     * @param object [in]   the object to perform the action on
     * @param action [in]   the action to perform
     */
    ScopedGuard(T *object, ActionType action)
        : m_object(object)
        , m_action(action)
    {
    }
    /**
     * Destructor
     */
    ~ScopedGuard()
    {
        leaveScope();
    }

    /**
     * Cancel the action associated with this instance.
     */
    void cancel()
    {
        m_action = NULL;
    }

private:
    T *m_object;
    ActionType m_action;

    /**
     * Called when leaving the scope. Calls action and resets the action.
     */
    void leaveScope()
    {
        if (m_action && m_object)
        {
            if (!(m_object->*m_action)())
                REPORT_ERROR("Action call failed");
            m_object = NULL;
            m_action = NULL;
        }
    }

    /**
     * Hide default/copy constructor and assignment operator
     */
    ScopedGuard();
    ScopedGuard(ScopedGuard &other);
    const ScopedGuard& operator = (const ScopedGuard&);
};

} // namespace ArgusSamples

#endif // SCOPED_GUARD_H
