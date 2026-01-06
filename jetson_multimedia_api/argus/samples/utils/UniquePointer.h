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

#ifndef UNIQUE_POINTER_H
#define UNIQUE_POINTER_H

#include <assert.h>

namespace ArgusSamples
{

/**
 * Gneric unique pointer implementation. A unique pointer owns an object through a pointer and
 * destroys that pointer when UniquePointer goes out of scope
 */
template <typename T> class UniquePointer
{
public:
    /**
     * Constructor
     */
    UniquePointer()
        : m_ptr(NULL)
    {
    }

    /**
     * Constructor of object owning 'ptr'.
     */
    explicit UniquePointer(T *ptr)
        : m_ptr(ptr)
    {
    }

    /**
     * Destructor, Destructs the owned object if such is present.
     */
    ~UniquePointer()
    {
        reset();
    }

    /**
     * Dereferences pointer to the owned object
     */
    T* operator->() const
    {
        return m_ptr;
    }

    /**
     * Dereferences pointer to the owned object
     */
    T* get() const
    {
        return m_ptr;
    }

    /**
     * Returns true if there is an owned object.
     */
#if (__cplusplus > 201100L)
    explicit operator bool() const
#else
    operator bool() const
#endif
    {
        return (m_ptr != NULL);
    }

    /**
     * Release the ownership of the object. Return the owned object.
     */
    T* release()
    {
        T *ptr = m_ptr;
        m_ptr = NULL;
        return ptr;
    }

    /**
     * Replace the owned object, the previously owned object is destructed.
     */
    void reset(T *ptr = NULL)
    {
        delete m_ptr;
        m_ptr = ptr;
    }

    /**
     * Get pointer to pointer to the owned object
     */
    T ** operator &()
    {
        // This is a special addition to be able to use this class with functions creating objects
        // e.g. 'bool func(Type **newObj)'. Make sure this is only used if there is no owned
        // object yet.
        assert(m_ptr == NULL);
        return &m_ptr;
    }

private:
    T *m_ptr;       ///< owned object

    /**
     * Hide copy constructor and assignment operator
     */
    UniquePointer(UniquePointer &right);
    UniquePointer& operator=(const UniquePointer &right);
};

} // namespace ArgusSamples

#endif // UNIQUE_POINTER_H
