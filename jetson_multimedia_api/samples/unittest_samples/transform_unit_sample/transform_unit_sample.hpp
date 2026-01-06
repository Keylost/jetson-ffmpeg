/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
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

#include "nvbuf_utils.h"

#define MAX_PLANES 4

/**
 * @brief Print the commandline arguments.
 *
 * Helper function to print the commandline arguments required by the application.
 *
 */
static void print_help(void);

/**
 * @brief Maps input pixel format to NvBufferColorFormat.
 *
 * Helper function to map user input format with the corresponding
 * NvBufferColorFormat.
 *
 * @param[in] userdefined_fmt User input pixel format
 * @param[in] pixel_format Pointer to the corresponding NvBufferColorFormat
 */
static void
get_color_format(const char* userdefined_fmt, NvBufferColorFormat* pixel_format);

/**
 * @brief Get bytes per pixel for each format.
 *
 * Helper function to get bytes per pixel required for each plane for the given format.
 *
 * @param[in] bytes_per_pixel_req Pointer to the bytes per pixel required
 * @param[in] pixel_format NvBufferColorFormat color
 */
void fillBytesperPixel(NvBufferColorFormat pixel_format, int * bytes_per_pixel_req);

/**
 * @brief Read the frame into source DMA buffer.
 *
 * Helper function to read the file into the source DMA buffer.
 *
 * @param[in] src_dma_fd Source DMA buffer FD
 * @param[in] input_stream Input stream
 */
static int read_video_frame(int src_dma_fd, std::ifstream * input_stream);

/**
 * @brief Write the transformed DMA buffer.
 *
 * Helper function to write the transformed DMA buffer into the
 * destination file.
 *
 * @param[in] dest_dma_fd Destination DMA buffer FD
 * @param[in] output_stream Input stream
 */
static int write_video_frame(int dest_dma_fd, std::ofstream * output_stream);
