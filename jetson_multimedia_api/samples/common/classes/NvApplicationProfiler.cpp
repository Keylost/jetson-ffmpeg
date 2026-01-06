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

#include "NvApplicationProfiler.h"
#include <fstream>
#include <sstream>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define GOVERNOR_SYS_FILE "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
#define CPU_FREQ_FILE "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq"
#define REQUIRED_GOVERNOR "performance"

#define TIMESPEC_DIFF_USEC(timespec1, timespec2) \
    (timespec1.tv_sec - timespec2.tv_sec) * 1000000.0 + \
    (timespec1.tv_nsec - timespec2.tv_nsec) / 1000.0

using namespace std;

NvApplicationProfiler::NvApplicationProfiler()
{
    char governor[64];
    uint64_t cpu_freq_khz;

    memset(&data, 0, sizeof(data));

    running = false;

    data.max_cpu_usage = 0;
    data.min_cpu_usage = 100;
    sampling_interval = DefaultSamplingInterval;

    profiling_thread = 0;
    pthread_mutex_init(&thread_lock, NULL);
    check_cpu_usage = true;

    ifstream cpu_governor_file(GOVERNOR_SYS_FILE, std::ifstream::in);
    cpu_governor_file >> governor;
    if (strcmp(governor, REQUIRED_GOVERNOR))
    {
        cerr << "Set governor to " REQUIRED_GOVERNOR " before enabling profiler"
            << endl;
        check_cpu_usage = false;
    }
    num_cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);

    ifstream cpu_freq_file(CPU_FREQ_FILE, std::ifstream::in);
    cpu_freq_file >> cpu_freq_khz;
    cpu_freq = cpu_freq_khz / 1000;
}

NvApplicationProfiler&
NvApplicationProfiler::getProfilerInstance()
{
    static NvApplicationProfiler profiler;
    return profiler;
}

void
NvApplicationProfiler::start(uint32_t sampling_interval_ms)
{

    pthread_mutex_lock(&thread_lock);

    if (running)
    {
        pthread_mutex_unlock(&thread_lock);
        return;
    }

    if (!check_cpu_usage)
    {
        cerr << "Set governor to " REQUIRED_GOVERNOR " before enabling profiler"
            << endl;
        pthread_mutex_unlock(&thread_lock);
        return;
    }

    running = true;
    sampling_interval = sampling_interval_ms;

    memset(&data, 0, sizeof(data));

    gettimeofday(&data.start_time, NULL);

    if (check_cpu_usage)
    {
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &data.start_proc_cpu_clock_time);
        clock_gettime(CLOCK_MONOTONIC, &data.start_cpu_clock_time);
    }

    pthread_create(&profiling_thread, NULL, ProfilerThread, this);
    pthread_setname_np(profiling_thread, "ProfilingThread");

    pthread_mutex_unlock(&thread_lock);
}

void
NvApplicationProfiler::stop()
{
    running = false;
    pthread_join(profiling_thread, NULL);

    pthread_mutex_lock(&thread_lock);
    gettimeofday(&data.stop_time, NULL);
    pthread_mutex_unlock(&thread_lock);
}

void
NvApplicationProfiler::profile()
{
    if (check_cpu_usage)
    {
        struct timespec cur_proc_cpu_clock_time;
        struct timespec cur_cpu_clock_time;

        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cur_proc_cpu_clock_time);
        clock_gettime(CLOCK_MONOTONIC, &cur_cpu_clock_time);

        if (data.num_readings)
        {
            float proc_cpu_time = TIMESPEC_DIFF_USEC(cur_proc_cpu_clock_time,
                    data.stop_proc_cpu_clock_time);

            float total_cpu_time = TIMESPEC_DIFF_USEC(cur_cpu_clock_time,
                    data.stop_cpu_clock_time);

            float cpu_usage = proc_cpu_time * 100 / total_cpu_time;
            if (cpu_usage < data.min_cpu_usage && cpu_usage > 0)
            {
                data.min_cpu_usage = cpu_usage;
            }
            if (cpu_usage > data.max_cpu_usage)
            {
                data.max_cpu_usage = cpu_usage;
            }
        }

        data.stop_proc_cpu_clock_time = cur_proc_cpu_clock_time;
        data.stop_cpu_clock_time = cur_cpu_clock_time;
    }
    data.num_readings++;
}

void *
NvApplicationProfiler::ProfilerThread(void * data)
{
    NvApplicationProfiler *profiler = (NvApplicationProfiler *) data;
    struct timespec next_profile_time;
    struct timeval now;
    pthread_cond_t sleep_cond;

    pthread_cond_init(&sleep_cond, NULL);

    gettimeofday(&now, NULL);
    next_profile_time.tv_sec = now.tv_sec;
    next_profile_time.tv_nsec = now.tv_usec * 1000L;

    pthread_mutex_lock(&profiler->thread_lock);
    while (profiler->running)
    {
        pthread_cond_timedwait(&sleep_cond, &profiler->thread_lock,
                &next_profile_time);
        profiler->profile();

        next_profile_time.tv_sec += profiler->sampling_interval / 1000;
        next_profile_time.tv_nsec += (profiler->sampling_interval % 1000) * 1000000L;
        next_profile_time.tv_sec += next_profile_time.tv_nsec / 1000000000L;
        next_profile_time.tv_nsec %= 1000000000L;

    }
    pthread_mutex_unlock(&profiler->thread_lock);

    return NULL;
}

void
NvApplicationProfiler::getProfilerData(NvAppProfilerData &pdata)
{
    pthread_mutex_lock(&thread_lock);

    memset (&pdata, 0, sizeof(pdata));

    if (check_cpu_usage)
    {
        float proc_cpu_time = TIMESPEC_DIFF_USEC(data.stop_proc_cpu_clock_time,
                data.start_proc_cpu_clock_time);

        float total_cpu_time = TIMESPEC_DIFF_USEC(data.stop_cpu_clock_time,
                data.start_cpu_clock_time);

        pdata.peak_cpu_usage = data.max_cpu_usage / num_cpu_cores;
        pdata.avg_cpu_usage = proc_cpu_time * 100 / total_cpu_time / num_cpu_cores;

        pdata.total_time.tv_sec = data.stop_time.tv_sec - data.start_time.tv_sec;
        pdata.total_time.tv_usec = data.stop_time.tv_usec - data.start_time.tv_usec;
        if (pdata.total_time.tv_usec < 0)
        {
            pdata.total_time.tv_sec--;
            pdata.total_time.tv_usec += 1000000;
        }

        pdata.num_cpu_cores = num_cpu_cores;
        pdata.cpu_freq_mhz = cpu_freq;
    }

    pthread_mutex_unlock(&thread_lock);
}
void
NvApplicationProfiler::printProfilerData(std::ostream &outstream)
{
    NvAppProfilerData data;
    getProfilerData(data);
    outstream << "************************************" << endl;
    outstream << "Total Profiling Time = " <<
        (data.total_time.tv_sec + 0.000001 * data.total_time.tv_usec) <<
        " sec" << endl;
    if (check_cpu_usage)
    {
        outstream << "Peak CPU Usage = " << data.peak_cpu_usage << "%" << endl;
        outstream << "Avg CPU Usage = " << data.avg_cpu_usage << "%" << endl;
        outstream << "Num. of Cores = " << data.num_cpu_cores << endl;
        outstream << "CPU frequency = " << data.cpu_freq_mhz << "MHz" << endl;
    }
    outstream << "************************************" << endl;
}

