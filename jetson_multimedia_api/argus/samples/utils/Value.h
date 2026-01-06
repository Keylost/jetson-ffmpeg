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

#ifndef VALUE_H
#define VALUE_H

#include <string>

#include "Observed.h"
#include "Validator.h"
#include "Error.h"

namespace ArgusSamples
{

/**
 * Value class. Validates the value when it's set. Notify observers when the value change.
 */
template<typename T> class Value : public Observed
{
public:
    Value(const T &value)
        : m_validator(new ValidatorNull<T>())
        , m_value(value)
    {
    }

    Value()
        : m_validator(new ValidatorNull<T>())
    {
    }

    Value(IValidator<T> *validator, const T &value)
        : m_validator(validator)
        , m_value(value)
    {
    }

    ~Value()
    {
        delete m_validator;
    }

    /**
     * Implicit conversion back to T
     */
    operator T const & () const
    {
        return m_value;
    }

    /**
     * Get the value.
     */
    const T & get() const
    {
        return m_value;
    }

    /**
     * Set the value. Check if the new value is valid, notify observers if the new value is
     * different from the old value.
     *
     * @param value [in] new value
     * @param forceNotify [in] notify observers even if the value has not changed
     */
    bool set(const T &value, bool forceNotify = false)
    {
        PROPAGATE_ERROR(m_validator->checkValid(value));
        if (!(value == m_value) || forceNotify)
        {
            m_value = value;
            PROPAGATE_ERROR(notifyObservers());
        }
        return true;
    }

    /**
     * Set the value using a string.
     *
     * @param valueString [in] new value represented by a string
     */
    bool setFromString(const char *valueString)
    {
        T value = m_value;
        PROPAGATE_ERROR(m_validator->toValue(valueString, value));
        PROPAGATE_ERROR(set(value));
        return true;
    }

    /**
     * Get the validator.
     */
    IValidator<T> *getValidator()
    {
        return m_validator;
    }

    /**
     * Convert to string
     */
    std::string toString() const
    {
        return m_validator->toString(m_value);
    }

private:
    IValidator<T> *m_validator;
    T m_value;

    // can't be copied
    Value(Value &other);
    const Value& operator = (const Value&);
};

}; // namespace ArgusSamples

#endif // VALUE_H
