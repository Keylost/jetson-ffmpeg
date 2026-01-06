/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION. All rights reserved.
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

#include <sys/time.h>
#include <stddef.h>
#include <cerrno>

#include <stdio.h>

#include "Util.h"
#include "Error.h"
#include "string.h"

namespace ArgusSamples
{

TimeValue getCurrentTime()
{
    struct timeval val;

    gettimeofday(&val, NULL);

    return
        TimeValue::fromUSec(val.tv_usec) +
        TimeValue::fromSec(static_cast<TimeValue::SecType>(val.tv_sec));
}

bool validateOutputPath(const char* filename)
{
    // Test ability to write to indicated filename
    FILE *fp = fopen(filename,"w");
    if (fp)
    {
        fclose(fp);
        return true;
    }
    else
    {
        fprintf(stderr, "--------------------------------------------------------------------\n");
        fprintf(stderr, "Failure to open file '%s' for writing.\n", filename);
        fprintf(stderr, "Error return code:%d (%s)\n", errno, strerror(errno));
        if (errno == EACCES)
        {
            fprintf(stderr, "Use command line parameter --outputpath to\n");
            fprintf(stderr, "point to a directory you have permission to write into.\n");
#ifdef __ANDROID__
            fprintf(stderr, "As in: argus_camera --outputpath='/sdcard/DCIM' -s1 -x\n");
#else
            fprintf(stderr, "As in: argus_camera --outputpath='/tmp' -s1 -x\n");
#endif
        }
        fprintf(stderr, "--------------------------------------------------------------------\n");
        return false;
    }
}

}; // namespace ArgusSamples
