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

#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <stdlib.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>   // for SCNu64
#include <assert.h>

#include <list>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <Argus/Argus.h>

#include "Error.h"
#include "Observed.h"
#include "IObserver.h"

namespace ArgusSamples
{

template<typename T> class Value;

/**
 * Convert from string to value.
 */
template<typename T> static inline bool convertToValue(const char *string, T &value)
{
    ORIGINATE_ERROR("Unsupported conversion");
}

/**
 * Convert from string to value, int32_t specialization.
 */
template<> inline bool convertToValue<>(const char *string, int32_t &value)
{
    if (sscanf(string, "%d", &value) != 1)
        ORIGINATE_ERROR("Invalid value '%s', expected signed integer.", string);

    return true;
}

/**
 * Convert from string to value, uint32_t specialization.
 */
template<> inline bool convertToValue<>(const char *string, uint32_t &value)
{
    // sscanf does not fail on signed values (returns two complement), therefore first convert to
    // a signed value and check for negative values
    int32_t signedValue;
    if (sscanf(string, "%d", &signedValue) == 1)
    {
        if (signedValue >= 0)
        {
            if (sscanf(string, "%u", &value) == 1)
                return true;
        }
    }

    ORIGINATE_ERROR("Invalid value '%s', expected unsigned integer.", string);
}

/**
 * Convert from string to value, float specialization.
 */
template<> inline bool convertToValue<>(const char *string, float &value)
{
    if (sscanf(string, "%f", &value) == 1)
        return true;

    ORIGINATE_ERROR("Invalid value '%s', expected float.", string);
}

/**
 * Convert from string to value, bool specialization.
 */
template<> inline bool convertToValue<>(const char *string, bool &value)
{
    // allow both '!=0'/'0', 'off'/'on' and 'false'/'true'
    uint32_t number = 0;
    if (sscanf(string, "%u", &number) == 1)
    {
        value = bool(number != 0);
        return true;
    }
    else if ((strcmp(string, "true") == 0) || (strcmp(string, "on") == 0))
    {
        value = true;
        return true;
    }
    else if ((strcmp(string, "false") == 0) || (strcmp(string, "off") == 0))
    {
        value = false;
        return true;
    }
    else
    {
        ORIGINATE_ERROR("The string '%s' is not a valid boolean value. Valid strings are "
            "'off'/'on', 'false'/'true' and integer numbers where 0 is 'false' and any other value "
            "is 'true'.", string);
    }
    return false;
}

/**
 * Convert from string to value, Argus::Range<float> specialization.
 */
template<> inline bool convertToValue<>(const char *string, Argus::Range<float> &value)
{
    if (sscanf(string, "%f,%f", &value.min(), &value.max()) == 2)
        return true;

    ORIGINATE_ERROR("Invalid value '%s', expected a range in format 'min,max'.", string);
}

/**
 * Convert from string to value, Argus::Range<uint64_t> specialization.
 */
template<> inline bool convertToValue<>(const char *string, Argus::Range<uint64_t> &value)
{
    if (sscanf(string, "%" SCNu64 ",%" SCNu64, &value.min(), &value.max()) == 2)
        return true;

    ORIGINATE_ERROR("Invalid value '%s', expected a range in format 'min,max'.", string);
}

/**
 * Convert from string to value, Argus::Size specialization.
 */
template<> inline bool convertToValue<>(const char *string, Argus::Size2D<uint32_t> &value)
{
    // sscanf does not fail on signed values (returns two complement), therefore first convert to
    // a signed value and check for negative values
    int32_t signedWidth, signedHeight;
    if (sscanf(string, "%dx%d", &signedWidth, &signedHeight) == 2)
    {
        if ((signedWidth >= 0) && (signedHeight >= 0))
        {
            if (sscanf(string, "%dx%d", &value.width(), &value.height()) == 2)
                return true;
        }
    }
    ORIGINATE_ERROR("Invalid value '%s', expected a size in format 'widthxheight'.", string);
}

/**
 * Convert from string to value, std::string specialization.
 */
template<> inline bool convertToValue<>(const char *string, std::string &value)
{
    value = string;
    return true;
}

/**
 * Convert from string to value, Argus::Rectangle specialization.
 */
template<> inline bool convertToValue<>(const char *string, Argus::Rectangle<uint32_t> &value)
{
    // sscanf does not fail on signed values (returns two complement), therefore first convert to
    // a signed value and check for negative values
    int32_t signedLeft, signedTop, signedWidth, signedHeight;
    if (sscanf(string, " %d , %d , %d , %d",
               &signedLeft, &signedTop, &signedWidth, &signedHeight) == 4)
    {
        if ((signedLeft >= 0) && (signedTop >= 0) && (signedWidth >= 0) && (signedHeight >= 0))
        {
            value.left()   = (uint32_t)signedLeft;
            value.top()    = (uint32_t)signedTop;
            value.right()  = (uint32_t)signedWidth + value.left();
            value.bottom() = (uint32_t)signedHeight + value.top();
            return true;
        }
    }

    ORIGINATE_ERROR("Invalid value '%s', expected rectangle in format 'left, top, width, height'.",
                    string);
}

/**
 * Convert from string to value, Argus::Rectangle specialization.
 */
template<> inline bool convertToValue<>(const char *string, Argus::Rectangle<float> &value)
{
    // sscanf does not fail on signed values (returns two complement), therefore first convert to
    // a signed value and check for negative values
    float left, top, width, height;
    if (sscanf(string, " %f , %f , %f , %f",
               &left, &top, &width, &height) == 4)
    {
        if ((left >= 0) && (top >= 0) && (width >= 0) && (height >= 0))
        {
            value.left()   = left;
            value.top()    = top;
            value.right()  = width + value.left();
            value.bottom() = height + value.top();
            return true;
        }
    }

    ORIGINATE_ERROR("Invalid value '%s', expected rectangle in format 'left, top, width, height'.",
                    string);
}

/**
 * Convert from string to value, std::vector device indexes specialization.
 */
template<> inline bool convertToValue<>(const char *string, std::vector<uint32_t> &value)
{
    const uint32_t len = 100;
    if (strlen(string) >= len)
    {
        ORIGINATE_ERROR("Invalid value '%s', expected length less than %u",
                        string, len);
    }

    char inputString[len];
    strncpy(inputString, string, len);

    char *save = NULL;
    char *ptr = strtok_r(inputString, ",", &save);
    while (ptr)
    {
        int32_t index = atoi(ptr);
        if (index < 0)
        {
            return false;
        }

        value.push_back((uint32_t)index);
        ptr = strtok_r(NULL, ",", &save);
    }

    return (value.size() > 0);
}

/**
 * Convert from value to string.
 */
template<typename T> static inline std::string convertToString(const T &value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

/**
 * Convert from range to string.
 */
template<typename T> static inline std::string convertToString(const Argus::Range<T> &value)
{
    std::ostringstream stream;
    stream << value.min() << "," << value.max();
    return stream.str();
}

/**
 * Convert from range-range to string.
 */
template<typename T> static inline std::string convertToString(const Argus::Range<Argus::Range<T> > &value)
{
    std::ostringstream stream;
    stream << "(" << convertToString(value.min()) << "),(" << convertToString(value.max()) << ")";
    return stream.str();
}

/**
 * Convert from value to string, Argus::Size specialization.
 */
template<> inline std::string convertToString<>(const Argus::Size2D<uint32_t> &value)
{
    std::ostringstream stream;
    stream << value.width() << "x" << value.height();
    return stream.str();
}

/**
 * Convert from value to string, Argus::Rectangle specialization.
 */
template<> inline std::string convertToString<>(const Argus::Rectangle<uint32_t> &value)
{
    std::ostringstream stream;
    stream << value.left() << "," << value.top() << "," << value.right() << "," << value.bottom();
    return stream.str();
}

/**
 * Convert from value to string, Argus::Rectangle specialization.
 */
template<> inline std::string convertToString<>(const Argus::Rectangle<float> &value)
{
    std::ostringstream stream;
    stream << value.left() << "," << value.top() << "," << value.right() << "," << value.bottom();
    return stream.str();
}

/**
 * Convert from value to string, std::vector multi devices index specilization
 */
template<> inline std::string convertToString<>(const std::vector<uint32_t> &value)
{
    std::ostringstream stream;
    std::vector<uint32_t>::const_iterator it;
    for (it = value.begin(); it != value.end(); ++it)
    {
        if ((it + 1) != value.end())
        {
            stream << *it << ",";
        }
        else
        {
            stream << *it;
        }
    }
    return stream.str();
}

/**
 * Value validator interface class. Provides a function to check if a value is valid.
 */
template<typename T> class IValidator : public Observed
{
public:
    IValidator() { }
    virtual ~IValidator() { }

    /**
     * Check if 'value' is valid.
     * @param [in] value
     */
    virtual bool checkValid(const T &value) const = 0;

    /**
     * Get a string describing the valid values.
     * @return Valid messages
     */
    virtual std::string getValidValuesMessage() const = 0;

    /**
     * Convert value to string
     */
    virtual std::string toString(const T &value) const = 0;

    /**
     * Convert string to value
     */
    virtual bool toValue(const char *string, T &value) const
    {
        return convertToValue(string, value);
    }

    /**
     * Get the smallest of the valid values. Only implemented for validators supporting a range
     * of values.
     */
    virtual bool getMin(T *min) const
    {
        ORIGINATE_ERROR("Not implemented (%s)", __FUNCTION__);
    }

    /**
     * Get the largest of the valid values. Only implemented for validators supporting a range
     * of values.
     */
    virtual bool getMax(T *max) const
    {
        ORIGINATE_ERROR("Not implemented (%s)", __FUNCTION__);
    }

    /**
     * Get an array of valid values. Only implemented for validators supporting
     * scattered values like enum validators.
     */
    virtual bool getValidValues(const std::vector<T> **values)
    {
        ORIGINATE_ERROR("Not implemented (%s)", __FUNCTION__);
    }
};

/**
 * Null validator, always signals validity.
 */
template<typename T> class ValidatorNull : public IValidator<T>
{
public:
    ValidatorNull() { }
    virtual ~ValidatorNull() { }

    /** @name IValidator methods */
    /**@{*/
    virtual bool checkValid(const T &value) const
    {
        return true;
    }
    virtual std::string getValidValuesMessage() const
    {
        return "";
    }
    virtual std::string toString(const T &value) const
    {
        return convertToString(value);
    }
    virtual bool getMin(T *min) const
    {
        return true;
    }
    virtual bool getMax(T *max) const
    {
        return true;
    }
    /**@}*/
};

/**
 * Enum validator
 */
template<typename T> class ValidatorEnum : public IValidator<T>
{
public:
    /**
     * A struct for a value and its string representation.
     */
    struct ValueStringPair
    {
        T value;
        std::string string;
    };

    ValidatorEnum()
    {
    }

    ValidatorEnum(const ValueStringPair *enums, size_t count)
    {
        PROPAGATE_ERROR_CONTINUE(setValidValues(enums, count));
    }

    virtual ~ValidatorEnum() { }

    /**
     * Set the valid values.
     */
    bool setValidValues(const ValueStringPair *enums, size_t count)
    {
        m_enums.clear();
        m_strings.clear();

        for (size_t index = 0; index < count; ++index)
        {
            m_enums.push_back(enums[index].value);
            m_strings.push_back(enums[index].string);
        }

        PROPAGATE_ERROR(Observed::notifyObservers());

        return true;
    }

    /**
     * Set the valid values.
     */
    bool setValidValues(const std::vector<T>& enums)
    {
        const size_t size = 100;
        char buffer[size];
        std::string tmp;
        int written = 0;

        m_enums.clear();
        m_strings.clear();

        for (size_t index = 0; index < enums.size(); ++index)
        {
            m_enums.push_back(enums[index]);
            written = snprintf(buffer, size, "%f", enums[index]);
            tmp.assign(buffer, written);
            m_strings.push_back(tmp);
        }

        PROPAGATE_ERROR(Observed::notifyObservers());

        return true;
    }

    /** @name IValidator methods */
    /**@{*/
    virtual bool checkValid(const T &value) const
    {
        for (size_t index = 0; index < m_enums.size(); ++index)
        {
            if (m_enums[index] == value)
                return true;
        }

        return false;
    }

    virtual std::string getValidValuesMessage() const
    {
        std::ostringstream stream;
        stream << "Valid strings are ";
        for (size_t index = 0; index < m_enums.size(); ++index)
        {
            if (index != 0)
                stream << ", ";
            stream << "'" << m_strings[index] << "'";
        }
        stream << " or an index in the range [0, " << m_enums.size()-1 << "].";
        return stream.str();
    }

    virtual std::string toString(const T &value) const
    {
        for (size_t index = 0; index < m_enums.size(); ++index)
        {
            if (m_enums[index] == value)
                return m_strings[index];
        }

        return std::string(" not found ");
    }

    virtual bool toValue(const char *string, T &value) const
    {
        for (size_t index = 0; index < m_enums.size(); ++index)
        {
            if (m_strings[index] == string)
            {
                value = m_enums[index];
                return true;
            }
        }

        // also accept an index
        uint32_t index;
        if (sscanf(string, "%u", &index) == 1)
        {
            if (index < m_enums.size())
            {
                value = m_enums[index];
                return true;
            }
        }

        std::ostringstream stream;
        stream << "The string '" << string << "' is not valid. " << getValidValuesMessage();
        ORIGINATE_ERROR("%s", stream.str().c_str());
    }

    virtual bool getValidValues(const std::vector<T> **values)
    {
        if (values == NULL)
            ORIGINATE_ERROR("'values' is NULL");

        *values = &m_enums;
        return true;
    }
/**@}*/

private:
    std::vector<T> m_enums;
    std::vector<std::string> m_strings;
};

/**
 * Compare ranges, assumes that min <= max.
 */
template<typename T> bool operator<(const Argus::Range<T> &l, const Argus::Range<T> &r)
{
    assert(l.min() <= l.max());
    assert(r.min() <= r.max());
    return l.min() < r.min();
}

/**
 * Compare ranges, assumes that min <= max.
 */
template<typename T> bool operator>(const Argus::Range<T> &l, const Argus::Range<T> &r)
{
    assert(l.min() <= l.max());
    assert(r.min() <= r.max());
    return l.max() > r.max();
}

/**
 * Range validator, signals validity if the value is inside the range. The range is either fixed
 * or variable when initialized with an pointer to an Value<Argus::Range<T>> variable.
 */
template<typename T> class ValidatorRange : public IValidator<T>, public IObserver
{
public:
    /**
     * Constructor with fixed range
     * @param [in] min  lower bound (included)
     * @param [in] max  upper bound (included)
     */
    ValidatorRange(T min, T max)
        : m_observed(NULL)
        , m_min(min)
        , m_max(max)
    {
    }

    /**
     * Constructor with variable range, range bounds are updated by observing 'range'.
     *
     * @param [in] range    pointer to Value<Argus::Range<T>> variable
     */
    ValidatorRange(Value<Argus::Range<T> > *observed)
        : m_observed(observed)
        , m_min(0)
        , m_max(0)
    {
        PROPAGATE_ERROR_CONTINUE(m_observed->registerObserver(this,
            static_cast<IObserver::CallbackFunction>(&ValidatorRange<T>::onRangeChanged)));
    }

    virtual ~ValidatorRange()
    {
        if (m_observed)
        {
            PROPAGATE_ERROR_CONTINUE(m_observed->unregisterObserver(this,
                static_cast<IObserver::CallbackFunction>(&ValidatorRange<T>::onRangeChanged)));
        }
    }

    /** @name IValidator methods */
    /**@{*/
    virtual bool checkValid(const T &value) const
    {
        T min(value), max(value);

        PROPAGATE_ERROR(getMin(&min));
        PROPAGATE_ERROR(getMax(&max));

        if ((value < min) || (value > max))
        {
            std::ostringstream stream;
            stream << "Value '" << toString(value) << "' out of range. " << getValidValuesMessage();
            ORIGINATE_ERROR("%s", stream.str().c_str());
        }

        return true;
    }

    virtual std::string getValidValuesMessage() const
    {
        std::ostringstream stream;
        T min(0), max(0);

        PROPAGATE_ERROR_CONTINUE(getMin(&min));
        PROPAGATE_ERROR_CONTINUE(getMax(&max));

        stream << "Valid values need to be in the range [" << toString(min) << ", " <<
            toString(max) << "].";
        return stream.str();
    }

    virtual std::string toString(const T &value) const
    {
        return convertToString(value);
    }

    bool getMin(T *min) const
    {
        if (min == NULL)
            ORIGINATE_ERROR("'min' is NULL");

        *min = m_min;
        return true;
    }

    bool getMax(T *max) const
    {
        if (max == NULL)
            ORIGINATE_ERROR("'max' is NULL");

        *max = m_max;
        return true;
    }
    /**@}*/

private:
    Value<Argus::Range<T> > *m_observed;
    T m_min;
    T m_max;

    bool onRangeChanged(const Observed &source)
    {
        assert(&source == m_observed);
        const Value<Argus::Range<T> > &range =
            static_cast<const Value<Argus::Range<T> >&>(source);

        m_min = range.get().min();
        m_max = range.get().max();
        PROPAGATE_ERROR(Observed::notifyObservers());
        return true;
    }
};

/**
 * Size2D validator, signals validity if the value is a valid size.
 */
template<typename T> class ValidatorSize2D : public IValidator<Argus::Size2D<T> >
{
public:
    /**
     * Constructor with valid sizes
     */
    ValidatorSize2D(const Argus::Size2D<T> *sizes, size_t count, bool allowArbitrarySizes)
        : m_allowArbitrarySizes(allowArbitrarySizes)
    {
        for (size_t index = 0; index < count; ++index)
        {
            m_sizes.push_back(sizes[index]);
        }
    }
    virtual ~ValidatorSize2D()
    {
    }

    /** @name IValidator methods */
    /**@{*/
    virtual bool checkValid(const Argus::Size2D<T> &value) const
    {
        if (m_allowArbitrarySizes)
            return true;

        for (size_t index = 0; index < m_sizes.size(); ++index)
        {
            if (m_sizes[index] == value)
                return true;
        }

        std::ostringstream stream;
        stream << "Value '" << toString(value) << "' is not a valid size. " <<
            getValidValuesMessage();
        ORIGINATE_ERROR("%s", stream.str().c_str());
        return false;
    }

    virtual std::string getValidValuesMessage() const
    {
        if (m_allowArbitrarySizes)
            return "";

        std::ostringstream stream;
        stream << "Valid sizes are ";
        for (size_t index = 0; index < m_sizes.size(); ++index)
        {
            if (index != 0)
                stream << ", ";
            stream << "'" << toString(m_sizes[index]) << "'";
        }
        stream << ".";
        return stream.str();
    }

    virtual std::string toString(const Argus::Size2D<T> &value) const
    {
        return convertToString(value);
    }

    virtual bool getValidValues(const std::vector<Argus::Size2D<T> > **values)
    {
        if (values == NULL)
            ORIGINATE_ERROR("'values' is NULL");

        *values = &m_sizes;
        return true;
    }
    /**@}*/

private:
    bool m_allowArbitrarySizes;
    std::vector<Argus::Size2D<T> > m_sizes;
};

/**
 * std::vector validator, signals validity if the value is inside the of the vector size.
 */
template<typename T, typename VectorT> class ValidatorStdVector : public IValidator<T>
{
public:
    /**
     * Constructor with vector, range bounds are fetched from 'vector' on validity check
     * @param [in] vector
     */
    ValidatorStdVector(std::vector<VectorT> *vector)
        : m_vector(vector)
    {
    }
    virtual ~ValidatorStdVector() { }

    /** @name IValidator methods */
    /**@{*/
    virtual bool checkValid(const T &value) const
    {
        T min, max;

        PROPAGATE_ERROR(getMin(&min));
        PROPAGATE_ERROR(getMax(&max));

        if ((value < min) || (value > max))
        {
            std::ostringstream stream;
            stream << "Value '" << value << "' out of range [" << min << ", " <<
                max << "]";
            ORIGINATE_ERROR("%s", stream.str().c_str());
        }

        return true;
    }

    virtual std::string getValidValuesMessage() const
    {
        std::ostringstream stream;
        T min, max;

        PROPAGATE_ERROR_CONTINUE(getMin(&min));
        PROPAGATE_ERROR_CONTINUE(getMax(&max));

        stream << "Valid values need to be in the range [" << toString(min) << ", " <<
            toString(max) << "].";
        return stream.str();
    }

    virtual std::string toString(const T &value) const
    {
        return convertToString(value);
    }

    bool getMin(T *min) const
    {
        if (min == NULL)
            ORIGINATE_ERROR("'min' is NULL");

        *min = 0;
        return true;
    }

    bool getMax(T *max) const
    {
        if (max == NULL)
            ORIGINATE_ERROR("'max' is NULL");

        if (m_vector->size() == 0)
            *max = 0;
        else
            *max = m_vector->size() - 1;
        return true;
    }
    /**@}*/

private:
    std::vector<VectorT> *m_vector;
};

}; // namespace ArgusSamples

#endif // VALIDATOR_H
