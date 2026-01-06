/*
 * Copyright (c) 2016-2023, NVIDIA CORPORATION. All rights reserved.
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

#include "NvJpegEncoder.h"
#include "NvLogging.h"
#include <string.h>
#include <malloc.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ROUND_UP_4(num)  (((num) + 3) & ~3)

#define CAT_NAME "JpegEncoder"

NvJPEGEncoder::NvJPEGEncoder(const char *comp_name)
    :NvElement(comp_name, valid_fields)
{
    memset(&cinfo, 0, sizeof(cinfo));
    memset(&jerr, 0, sizeof(jerr));
    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_compress(&cinfo);
    jpeg_suppress_tables(&cinfo, TRUE);
}

NvJPEGEncoder *
NvJPEGEncoder::createJPEGEncoder(const char *comp_name)
{
    NvJPEGEncoder *jpegenc = new NvJPEGEncoder(comp_name);
    if (jpegenc->isInError())
    {
        delete jpegenc;
        return NULL;
    }
    return jpegenc;
}

NvJPEGEncoder::~NvJPEGEncoder()
{
    jpeg_destroy_compress(&cinfo);
    CAT_DEBUG_MSG(comp_name << " (" << this << ") destroyed");
}

int
NvJPEGEncoder::encodeFromFd(int fd, J_COLOR_SPACE color_space,
        unsigned char **out_buf, unsigned long &out_buf_size,
        int quality)
{
    uint32_t buffer_id;
    unsigned long out_buf_size_orig = out_buf_size;

    if (fd == -1)
    {
        COMP_ERROR_MSG("Not encoding because fd = -1");
        return -1;
    }

    buffer_id = profiler.startProcessing();

    jpeg_mem_dest(&cinfo, out_buf, &out_buf_size);

    cinfo.fd = fd;
    cinfo.IsVendorbuf = TRUE;

    cinfo.raw_data_in = TRUE;
    cinfo.in_color_space = JCS_YCbCr;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_set_hardware_acceleration_parameters_enc(&cinfo, TRUE, out_buf_size, 0, 0);

    switch (color_space)
    {
        case JCS_YCbCr:
            cinfo.in_color_space = JCS_YCbCr;
            break;
        default:
            COMP_ERROR_MSG("Color format " << color_space << " not supported\n");
            return -1;
    }

    jpeg_start_compress (&cinfo, 0);

    if (cinfo.err->msg_code)
    {
        char err_string[256];
        cinfo.err->format_message((j_common_ptr) &cinfo, err_string);
        COMP_ERROR_MSG ("Error in jpeg_start_compress: " << err_string);
        return -1;
    }

    jpeg_write_raw_data (&cinfo, NULL, 0);
    jpeg_finish_compress(&cinfo);
    /* If the output buffer size allocated is insufficient, libjpeg allocates more memory
     * and buffer pointers and size are updated accordingly.
     */
    if (out_buf_size_orig < (unsigned long)cinfo.outputBuffSize)
    {
        COMP_DEBUG_MSG("Insufficient output size! Reallocated to %u size\n"
                                    << cinfo.outputBuffSize);
        out_buf_size = (unsigned long)cinfo.outputBuffSize;
        *out_buf = cinfo.dest->next_output_byte;
    }

    COMP_DEBUG_MSG("Succesfully encoded Buffer fd=" << fd);

    profiler.finishProcessing(buffer_id, false);

    return 0;
}

int
NvJPEGEncoder::encodeFromBuffer(NvBuffer & buffer, J_COLOR_SPACE color_space,
        unsigned char **out_buf, unsigned long &out_buf_size,
        int quality)
{
    unsigned char **line[3];

    uint32_t comp_height[MAX_CHANNELS];
    uint32_t comp_width[MAX_CHANNELS];
    uint32_t h_samp[MAX_CHANNELS];
    uint32_t v_samp[MAX_CHANNELS];
    uint32_t h_max_samp = 0;
    uint32_t v_max_samp = 0;
    uint32_t channels;

    unsigned char *base[MAX_CHANNELS], *end[MAX_CHANNELS];
    unsigned int stride[MAX_CHANNELS];

    uint32_t width;
    uint32_t height;

    uint32_t i, j, k;
    uint32_t buffer_id;
    unsigned long out_buf_size_orig = out_buf_size;

    buffer_id = profiler.startProcessing();

    jpeg_mem_dest(&cinfo, out_buf, &out_buf_size);
    width = buffer.planes[0].fmt.width;
    height = buffer.planes[0].fmt.height;

    switch (color_space)
    {
        case JCS_YCbCr:
            channels = 3;

            comp_width[0] = width;
            comp_height[0] = height;

            comp_width[1] = width / 2;
            comp_height[1] = height / 2;

            comp_width[2] = width / 2;
            comp_height[2] = height / 2;

            break;
        default:
            COMP_ERROR_MSG("Color format " << color_space <<
                           " not supported\n");
            return -1;
    }

    if (channels != buffer.n_planes)
    {
        COMP_ERROR_MSG("Buffer not in proper format");
        return -1;
    }

    for (i = 0; i < channels; i++)
    {
        if (comp_width[i] != buffer.planes[i].fmt.width ||
            comp_height[i] != buffer.planes[i].fmt.height)
        {
            COMP_ERROR_MSG("Buffer not in proper format");
            return -1;
        }
    }

    h_max_samp = 0;
    v_max_samp = 0;

    for (i = 0; i < channels; ++i)
    {
        h_samp[i] = ROUND_UP_4(comp_width[0]) / comp_width[i];
        h_max_samp = MAX(h_max_samp, h_samp[i]);
        v_samp[i] = ROUND_UP_4(comp_height[0]) / comp_height[i];
        v_max_samp = MAX(v_max_samp, v_samp[i]);
    }

    for (i = 0; i < channels; ++i)
    {
        h_samp[i] = h_max_samp / h_samp[i];
        v_samp[i] = v_max_samp / v_samp[i];
    }

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = channels;
    cinfo.in_color_space = color_space;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_set_hardware_acceleration_parameters_enc(&cinfo, TRUE, out_buf_size, 0, 0);
    cinfo.raw_data_in = TRUE;

    if (cinfo.in_color_space == JCS_RGB)
        jpeg_set_colorspace(&cinfo, JCS_RGB);

    switch (color_space)
    {
        case JCS_YCbCr:
            cinfo.in_color_space = JCS_YCbCr;
            break;
        default:
            COMP_ERROR_MSG("Color format " << color_space << " not supported\n");
            return -1;
    }

    for (i = 0; i < channels; i++)
    {
        cinfo.comp_info[i].h_samp_factor = h_samp[i];
        cinfo.comp_info[i].v_samp_factor = v_samp[i];
        line[i] = (unsigned char **) malloc(v_max_samp * DCTSIZE *
                sizeof(unsigned char *));
    }

    for (i = 0; i < channels; i++)
    {
        base[i] = (unsigned char *) buffer.planes[i].data;
        stride[i] = buffer.planes[i].fmt.stride;
        end[i] = base[i] + comp_height[i] * stride[i];
    }

    jpeg_start_compress(&cinfo, TRUE);

    if (cinfo.err->msg_code)
    {
        char err_string[256];
        cinfo.err->format_message((j_common_ptr) &cinfo, err_string);
        COMP_ERROR_MSG ("Error in jpeg_start_compress: " << err_string);
        return -1;
    }

    for (i = 0; i < height; i += v_max_samp * DCTSIZE)
    {
        for (k = 0; k < channels; k++)
        {
            for (j = 0; j < v_samp[k] * DCTSIZE; j++)
            {
                line[k][j] = base[k];
                if (base[k] + stride[k] < end[k])
                    base[k] += stride[k];
            }
        }
        jpeg_write_raw_data(&cinfo, line, v_max_samp * DCTSIZE);
    }

    jpeg_finish_compress(&cinfo);
    /* If the output buffer size allocated is insufficient, libjpeg allocates more memory
     * and buffer pointers and size are updated accordingly.
     */
    if (out_buf_size_orig < (unsigned long)cinfo.outputBuffSize)
    {
        COMP_DEBUG_MSG("Insufficient output size! Reallocated to %u size\n"
                                    << cinfo.outputBuffSize);
        out_buf_size = (unsigned long)cinfo.outputBuffSize;
        *out_buf = cinfo.dest->next_output_byte;
    }
    for (i = 0; i < channels; i++)
    {
        free(line[i]);
    }
    COMP_DEBUG_MSG("Succesfully encoded Buffer");

    profiler.finishProcessing(buffer_id, false);

    return 0;
}

void
NvJPEGEncoder::setCropRect(uint32_t left, uint32_t top, uint32_t width,
        uint32_t height)
{
    cinfo.crop_rect.left = left;
    cinfo.crop_rect.top = top;
    cinfo.crop_rect.width = width;
    cinfo.crop_rect.height = height;
}


void
NvJPEGEncoder::setScaledEncodeParams(uint32_t scale_width, uint32_t scale_height)
{
    cinfo.image_scale = TRUE;
    cinfo.scaled_image_width = scale_width;
    cinfo.scaled_image_height = scale_height;
}
