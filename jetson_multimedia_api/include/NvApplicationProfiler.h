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


/**
 * @file
 * <b>NVIDIA Multimedia API: Application Resource Profiling API</b>
 *
 * @b Description: This file declares the NvApplicationProfiler API.
 */
#ifndef __NV_PROFILER_H__
#define __NV_PROFILER_H__

#include <iostream>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

/**
 *
 * Helper class for profiling the overall application
 * resource usage.
 *
 * NvApplicationProfiler spawns a background thread which periodically measures resource
 * usage. This sampling interval can be configured. Smaller sampling intervals
 * may lead to more accurate results but the background thread itself will
 * have a higher CPU usage.
 *
 * Only one instance of NvApplicationProfiler object gets created for the application.
 * It can be accessed using getProfilerInstance().
 *
 * NvApplicationProfiler currently samples CPU usage and provides peak and average CPU
 * usage during the profiling duration. It requires that the CPU frequency be
 * constant over the entire duration. To force this, NvApplicationProfiler will start only
 * when the CPU governor is set to @b performance.
 *
 * @defgroup l4t_mm_nvapplicationprofiler_group  Application Resource Profiler API
 * @ingroup aa_framework_api_group
 * @{
 */
class NvApplicationProfiler
{
public:
    /**
     * Holds the profiling data.
     */
    typedef struct
    {
        /** Total time for which the profiler ran or is running. */
        struct timeval total_time;
        /** Peak CPU usage during the profiling time. */
        float peak_cpu_usage;
        /** Average CPU usage over the entire profiling duration. */
        float avg_cpu_usage;
        /** Number of cpu cores. */
        uint32_t num_cpu_cores;
        /** Operating frequency of the cpu in MHz. */
        uint32_t cpu_freq_mhz;
    } NvAppProfilerData;

    static const uint64_t DefaultSamplingInterval = 100;

    /**
     * Gets a reference to the global #NvApplicationProfiler instance.
     *
     * @return A reference to the global NvApplicationProfiler instance.
     */
    static NvApplicationProfiler& getProfilerInstance();

    /**
     * Starts the profiler with the specified sampling interval.
     *
     * This method resets the internal profiler data measurements.
     *
     * Starting an already started profiler does nothing.
     *
     * @param[in] sampling_interval_ms Sampling interval in milliseconds.
     */
    void start(uint32_t sampling_interval_ms);

    /**
     * Stops the profiler.
     */
    void stop();

    /**
     * Prints the profiler data to an output stream.
     *
     * @param[in] outstream A reference to an output stream of type std::ostream.
     *                      std::cout if not specified.
     */
    void printProfilerData(std::ostream &outstream = std::cout);

    /**
     * Gets the profiler data.
     *
     * @param[out] data Pointer to the ProfilerData structure to be filled.
     */
    void getProfilerData(NvAppProfilerData &data);

private:
    /**
     * Method run by the background profiling thread.
     *
     * Waits on a condition till the next periodic sampling time and calls
     * profile() for measurement.
     *
     * Runs in an infinite loop till signaled to stop.
     */
    static void * ProfilerThread(void *);

    bool running;  /**< Boolean flag indicating if profiling thread is running. */
    uint32_t sampling_interval; /**< Interval between two measurements,
                                     in milliseconds. */

    pthread_mutex_t thread_lock; /**< Lock for synchronized multithreaded
                                      access to NvApplicationProfiler::data */
    pthread_t profiling_thread;  /** ID of the profiling thread running in
                                     background */

    uint32_t num_cpu_cores; /**< Number of CPU cores. */
    uint32_t cpu_freq; /**< Operating frequency of CPU cores in MHz. */
    bool check_cpu_usage; /**< Flag indicating if cpu usage should be checked. */

    /**
     * Holds resource usage readings (internal use only).
     */
    struct ProfilerDataInternal
    {
        /** Wall-clock time at which profiler was started. */
        struct timeval start_time;
        /** Wall-clock time at which profiler was stopped. */
        struct timeval stop_time;

        /** CPU clock time occupied by the process when the
         *  profiler was started. */
        struct timespec start_proc_cpu_clock_time;
        /** Total CPU clock time when the profiler was started. */
        struct timespec start_cpu_clock_time;

        /** CPU clock time occupied by the process when the
         *  latest readings were taken. */
        struct timespec stop_proc_cpu_clock_time;
        /** Total CPU clock time when the latest readings were taken. */
        struct timespec stop_cpu_clock_time;

        /** Maximum of CPU usages of all sampled periods. */
        float max_cpu_usage;
        /** Minimum of CPU usages of all sampled periods. */
        float min_cpu_usage;
        /** Average CPU usage over the entire profiling duration. */
        float avg_cpu_usage;

        /** Number of readings taken. */
        uint64_t num_readings;
    } data; /**< Internal structure to hold intermediate measurements. */

    /**
     * Measures resource usage parameters.
     */
    void profile();

    /**
     * Default constructor used by getProfilerInstance.
     */
    NvApplicationProfiler();

    /**
     * Disallows copy constructor.
     */
    NvApplicationProfiler(const NvApplicationProfiler& that);
    /**
     * Disallows assignment.
     */
    void operator=(NvApplicationProfiler const&);
};

/** @} */

#endif
