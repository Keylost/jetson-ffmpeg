/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION. All rights reserved.
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
 * <b>NVIDIA Multimedia API: Utility Functions</b>
 *
 * @b Description: This file declares the video frame utility functions.
 */

/**
 * @defgroup l4t_mm_nvutils_group Video Read/Write
 * @ingroup l4t_mm_nvvideo_group
 *
 * Utility functions for reading video frames from a file to the buffer
 * structure and for writing from the buffer structure to a file.
 *
 * @{
 */

#ifndef __NV_UTILS_H_
#define __NV_UTILS_H_

#include <fstream>
#include "NvBuffer.h"

/**
 * @brief Reads a video frame from a file to the buffer structure.
 *
 * This function reads data from the file into the buffer, one data plane
 * at a time. For each data plane, the number of bytes read is:
 *
 * @code stride * height @endcode
 * or
 * @code width * height * bytes_per_pixel @endcode
 *
 * @param[in] stream A pointer to the input file stream.
 * @param[in] buffer A reference to the buffer object into which data is read.
 * @return 0 if successful, or -1 otherwise.
 */
int read_video_frame(std::ifstream * stream, NvBuffer & buffer);

/**
 * @brief Writes a video frame from the buffer structure to a file.
 *
 * This function writes data to the file from the buffer, one plane at a time.
 * For each data plane, the number of bytes written is:
 *
 * @code stride * height @endcode
 * or
 * @code width * height * bytes_per_pixel @endcode
 *
 * @param[in] stream A pointer to the output file stream.
 * @param[in] buffer A reference to the buffer object from which
 *                   data is written.
 * @return 0 if successful, or -1 otherwise.
 */
int write_video_frame(std::ofstream * stream, NvBuffer & buffer);

/**
 * @brief Writes a plane of data from the buffer to a file.
 *
 * @param[in] dmabuf_fd     The DMABUF file descriptor of the buffer.
 * @param[in] plane         Index of the video frame plane.
 * @param[in] stream        A pointer to the output file stream.
 * @return 0 if successful, or -1 otherwise.
 */
int dump_dmabuf(int dmabuf_fd, unsigned int plane, std::ofstream * stream);

/**
 * @brief Parses the reference recon file to write the Y, U and V checksums.
 *
 * This function parses Y, U and V checksums from the reference recon file.
 *
 * @param[in] stream       A pointer to the input recon file stream.
 * @param[in] recon_params A pointer to an array in which to store
 *                          the parsed Y, U and V strings.
 * @return 0 for success, -1 otherwise.
 */
int parse_csv_recon_file(std::ifstream * stream, std::string * recon_params);
/** @} */
#endif
