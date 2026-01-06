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

#ifndef CAMERA_MODULES_ORDERED_H
#define CAMERA_MODULES_ORDERED_H

namespace ArgusSamples
{

/**
 * Used for variables shared by threads. Atomic access does inter-thread synchronization, writes
 * from one thread are guaranteed to be visible by other threads.
 */
template <typename T> class Ordered
{
public:
    Ordered(T value)
        : m_value(value)
    {
    }

    void set(T newValue)
    {
        m_value = newValue;
        __sync_synchronize();
    }

    T operator = (T newValue)
    {
        set(newValue);
        return m_value;
    }

    T get() const
    {
        return m_value;
    }

    operator T() const
    {
        return get();
    }

    T operator++()
    {
        return __sync_add_and_fetch(&m_value, 1);
    }

    T operator--()
    {
        return __sync_add_and_fetch(&m_value, -1);
    }

    bool compareExchange(T expectedValue, T newValue)
    {
        return (__sync_val_compare_and_swap(&m_value, expectedValue, newValue) == expectedValue);
    }

private:
    volatile T m_value;

    Ordered(Ordered &other);
    Ordered& operator=(const Ordered&);
};

} // namespace ArgusSamples

#endif // CAMERA_MODULES_ORDERED_H
