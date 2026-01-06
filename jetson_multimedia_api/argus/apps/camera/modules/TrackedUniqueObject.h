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

#ifndef ARGUS_APPS_CAMERA_UI_COMMON_TRACKEDUNIQUEOBJECT_H
#define ARGUS_APPS_CAMERA_UI_COMMON_TRACKEDUNIQUEOBJECT_H

#include "Error.h"

namespace ArgusSamples
{

/**
 * A class tracking an Argus object through the TrackedUniqueObj below. The track/untrack functions
 * are called whenever an Argus object is assigend to/removed from the TrackedUniqueObj object.
 */
template <typename T> class Tracker
{
public:
    virtual ~Tracker()
    {
    }

    /**
     * Called when the object is assigned to a TrackedUniqueObj
     */
    virtual bool track(T *obj) = 0;

    /**
     * Called when the object is removed from a TrackedUniqueObj
     */
    virtual bool untrack(T *ob) = 0;
};

/**
 * This class helps track construction and destruction of Argus objects. Tracker classes are
 * using it to track Argus objects.
 * It exposes several functions of the Argus::UniqueObj class to allow it to be seamlessly
 * replaced by a TrackedUniqueObj.
 */
template <typename T> class TrackedUniqueObj
{
public:
    TrackedUniqueObj()
        : m_tracker(NULL)
    {
    }

    ~TrackedUniqueObj()
    {
        PROPAGATE_ERROR_CONTINUE(reset());
    }

    T* get() const
    {
        return m_obj.get();
    }

    bool reset(T *obj = NULL, Tracker<T> *tracker = NULL)
    {
        if (obj && !tracker)
            ORIGINATE_ERROR("Need to specify a tracker if the object is set.");

        // untrack the previous object
        if (m_obj)
        {
            assert(m_tracker);
            PROPAGATE_ERROR(m_tracker->untrack(m_obj.get()));
        }

        m_obj.reset(obj);

        // track the new object
        if (m_obj)
        {
            assert(tracker);
            m_tracker = tracker;

            PROPAGATE_ERROR(m_tracker->track(m_obj.get()));
        }

        return true;
    }

    operator bool() const
    {
        return !!m_obj;
    }

    // allow this class to access private track/untrack functions from the Tracker class
    friend class Tracker<T>;

private:
    Tracker<T> *m_tracker;
    Argus::UniqueObj<T> m_obj;
};

}; // namespace ArgusSamples

#endif // ARGUS_APPS_CAMERA_UI_COMMON_TRACKEDUNIQUEOBJECT_H
