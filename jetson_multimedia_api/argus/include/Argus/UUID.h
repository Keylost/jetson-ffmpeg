/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION. All rights reserved.
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

/**
 * @file
 * <b>Libargus API: UUID API</b>
 *
 * @b Description: Defines the UUID types used by libargus.
 */

#ifndef _ARGUS_UUID_H
#define _ARGUS_UUID_H

#include <stdint.h>
#include <cstring>

namespace Argus
{

const uint32_t MAX_UUID_NAME_SIZE = 32;

/**
 * A universally unique identifier.
 */
struct UUID
{
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_hi_and_version;
    uint16_t clock_seq;
    uint8_t  node[6];

    bool operator==(const UUID &r) const
    {
        return memcmp(this, &r, sizeof(UUID)) == 0;
    }

    bool operator<(const UUID &r) const
    {
        return memcmp(this, &r, sizeof(UUID)) < 0;
    }
};

/**
 * A universally unique identifier with a name (used for debugging purposes).
 */
class NamedUUID : public UUID
{
public:
    NamedUUID(uint32_t time_low_
            , uint16_t time_mid_
            , uint16_t time_hi_and_version_
            , uint16_t clock_seq_
            , uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5
            , const char* name)
    {
        time_low = time_low_;
        time_mid = time_mid_;
        time_hi_and_version = time_hi_and_version_;
        clock_seq = clock_seq_;
        node[0] = c0; node[1] = c1; node[2] = c2; node[3] = c3; node[4] = c4; node[5] = c5;
        strncpy(m_name, name, sizeof(m_name)-1);
        m_name[sizeof(m_name)-1] = '\0';
    }

    NamedUUID(const NamedUUID& copied)
    : UUID(copied)
    {
        strncpy(m_name, copied.m_name, sizeof(m_name)-1);
        m_name[sizeof(m_name)-1] = '\0';
    }

    NamedUUID& operator=(const NamedUUID& copied)
    {
        static_cast<UUID&>(*this) = copied;

        strncpy(m_name, copied.m_name, sizeof(m_name)-1);
        m_name[sizeof(m_name)-1] = '\0';

        return *this;
    }

    bool operator==(const NamedUUID& compared) const
    {
        return static_cast<const UUID&>(*this) == compared;
    }

    bool operator!=(const NamedUUID& compared) const
    {
        return !(static_cast<const UUID&>(*this) == compared);
    }

    const char* getName() const { return m_name; }

private:
    char m_name[MAX_UUID_NAME_SIZE];

    NamedUUID();
};

/// Helper macro used to define NamedUUID-derived values.
#define DEFINE_UUID(TYPE, NAME, l, s0, s1, s2, c0,c1,c2,c3,c4,c5) \
    static const TYPE NAME(0x##l, 0x##s0, 0x##s1, 0x##s2, \
                           0x##c0, 0x##c1, 0x##c2, 0x##c3, 0x##c4, 0x##c5, #NAME);

#define DEFINE_NAMED_UUID_CLASS(NAME) \
        class NAME : public NamedUUID \
        { \
        public: \
            NAME(uint32_t time_low_ \
                        , uint16_t time_mid_ \
                        , uint16_t time_hi_and_version_ \
                        , uint16_t clock_seq_ \
                        , uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5 \
                        , const char* name) \
            : NamedUUID(time_low_, time_mid_, time_hi_and_version_, clock_seq_, \
                        c0, c1, c2, c3, c4, c5, name) \
            {} \
        private: \
            NAME();\
        };

} // namespace Argus

#endif // _ARGUS_UUID_H
