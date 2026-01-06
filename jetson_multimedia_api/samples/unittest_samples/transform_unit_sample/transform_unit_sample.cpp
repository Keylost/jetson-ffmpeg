/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
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
 * Execution command
 * ./transform_sample input_raw input_format width height output_raw output_format
 * Example:
 * ./transform_sample test_file.yuv yuv420 1920 1080 converted_file.yuv nv12
**/

#include <fstream>
#include <string>
#include <iostream>
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include <sstream>

using namespace std;

#include "transform_unit_sample.hpp"

/**
 * Video Transform/Filter using NVIDIA buffering utility.
 *
 * The NVIDIA buffering utility or nvbuf_utils provide a wrapper to simplify
 * the use case of applications/plugins for Buffering and
 * Transform or Composite or Blending.
 *
 * This is one such sample demonstration to use nvbuf_utils for conversion from
 * one pixel-format to another.
 * Supported pixel-formats, filters, composites and other properties are
 * described in the nvbuf_utils header.
 *
 * For transformation:
 * ## Specify parameters of input and output in NvBufferCreateParams for
 *    hardware buffers creation.
 * ## Create the HW buffer calling NvBufferCreateEx, which returns the
 *    DMABUF FD of the buffer allocated.
 * ## Define the transformation parameters in NvBufferTransformParams.
 * ## Call the NvBufferTransform which transforms the input DMA buffer
 *    to the output DMA buffer, both exported as fd.
**/

#define CHECK_ERROR(condition, error_str, label) if (condition) { \
                                                        cerr << error_str << endl; \
                                                        in_error = 1; \
                                                        goto label; }

static int bytes_per_pixel_destfmt[MAX_PLANES] = {0};
static int bytes_per_pixel_srcfmt[MAX_PLANES] = {0};

static void
print_help(void)
{
    cout << "Help:" << endl;
    cout << "Execution cmd:\n"
         << "./transform_sample input_file.yuv input_pixfmt "
         << "width height output_file.yuv output_pixfmt\n"
         << endl;
    cout << "Supported pixel formats:\n"
         << "\tnv12  nv21  nv12_709\n"
         << "\targb32  xrgb32\n"
         << "\tyuv420  yvu420  yuv420_709 yuv444"
         << endl;
}

static void
get_color_format(const char* userdefined_fmt, NvBufferColorFormat* pixel_format)
{
    if (!strcmp(userdefined_fmt, "nv12"))
        *pixel_format = NvBufferColorFormat_NV12;
    else if (!strcmp(userdefined_fmt, "nv21"))
        *pixel_format = NvBufferColorFormat_NV21;
    else if (!strcmp(userdefined_fmt,"nv12_709"))
        *pixel_format = NvBufferColorFormat_NV12_709;
    else if (!strcmp(userdefined_fmt,"argb32"))
        *pixel_format = NvBufferColorFormat_ARGB32;
    else if (!strcmp(userdefined_fmt,"xrgb32"))
        *pixel_format = NvBufferColorFormat_XRGB32;
    else if (!strcmp(userdefined_fmt,"yuv420"))
        *pixel_format = NvBufferColorFormat_YUV420;
    else if (!strcmp(userdefined_fmt,"yvu420"))
        *pixel_format = NvBufferColorFormat_YVU420;
    else if (!strcmp(userdefined_fmt,"yuv420_709"))
        *pixel_format = NvBufferColorFormat_YUV420_709;
    else if (!strcmp(userdefined_fmt,"yuv444"))
        *pixel_format = NvBufferColorFormat_YUV444;
    else
        *pixel_format = NvBufferColorFormat_Invalid;

}

static void
fill_bytes_per_pixel(NvBufferColorFormat pixel_format, int * bytes_per_pixel_req)
{
    switch (pixel_format)
    {
        case NvBufferColorFormat_NV12:
        case NvBufferColorFormat_NV21:
        case NvBufferColorFormat_NV12_709:
        {
            bytes_per_pixel_req[0] = 1;
            bytes_per_pixel_req[1] = 2;
            break;
        }
        case NvBufferColorFormat_NV21_12LE:
        {
            bytes_per_pixel_req[0] = 2;
            bytes_per_pixel_req[1] = 4;
            break;
        }
        case NvBufferColorFormat_ARGB32:
        case NvBufferColorFormat_XRGB32:
        {
            bytes_per_pixel_req[0] = 4;
            break;
        }
        case NvBufferColorFormat_YUV420:
        case NvBufferColorFormat_YVU420:
        case NvBufferColorFormat_YUV420_709:
        case NvBufferColorFormat_YUV444:
        {
            bytes_per_pixel_req[0] = 1;
            bytes_per_pixel_req[1] = 1;
            bytes_per_pixel_req[2] = 1;
            break;
        }
        default:
            return;
    }
}

/**
 * This function reads the video frame from the input file stream
 * and writes to the source HW buffer exported as FD.
 * Using the FD, HW buffer parameters are filled by calling
 * NvBufferGetParams. The parameters recieved from the buffer are
 * then used to write the raw stream in planar form into the buffer.
 *
 * For writing in the HW buffer:
 * A void data-pointer in created which stores the memory-mapped
 * virtual addresses of the planes.
 * For each plane, NvBufferMemMap is called which gets the
 * memory-mapped virtual address of the plane with the access
 * pointed by the flag in the void data-pointer.
 * Before the mapped memory is accessed, a call to NvBufferMemSyncForDevice()
 * with the virtual address must be present, before any modification
 * from CPU to the buffer is performed.
 * After writing the data, the memory-mapped virtual address of the
 * plane is unmapped.
**/

static int
read_video_frame(int src_dma_fd, ifstream * input_stream)
{
    int ret = 0;
    NvBufferParams src_param;
    ret = NvBufferGetParams (src_dma_fd, &src_param );
    if (ret)
    {
        cerr << "Get Params failed" << endl;
        return -1;
    }

    /* Void data pointer to store memory-mapped
    ** virtual addresses of the planes.
    */
    void *virtualip_data_addr;
    unsigned int plane = 0;

    for (plane = 0; plane < src_param.num_planes ; ++plane)
    {
        ret = NvBufferMemMap (src_dma_fd, plane, NvBufferMem_Write, &virtualip_data_addr);
        if (ret == 0)
        {
            unsigned int i = 0;
            for (i = 0; i < src_param.height[plane]; ++i)
            {
                streamsize bytes_to_read = src_param.width[plane] * bytes_per_pixel_srcfmt[plane];
                input_stream->read ((char*)virtualip_data_addr + i * src_param.pitch[plane],
                    bytes_to_read);
                if (input_stream->gcount() < bytes_to_read)
                {
                    cout << "End of File" << endl;
                    return -1;
                }
            }
            /* Syncs HW memory for writing to the buffer.
            ** This call must be called before any HW device
            ** accesses the buffer.
            */
            NvBufferMemSyncForDevice (src_dma_fd, plane, &virtualip_data_addr);
        }
        NvBufferMemUnMap (src_dma_fd, plane, &virtualip_data_addr);
    }
    return 0;
}

/**
 * This function writes the video frame from the HW buffer
 * exported as FD into the destination file.
 * Using the FD, HW buffer parameters are filled by calling
 * NvBufferGetParams. The parameters recieved from the buffer are
 * then used to read the planar stream from the HW buffer into the
 * output filestream.
 *
 * For reading from the HW buffer:
 * A void data-pointer in created which stores the memory-mapped
 * virtual addresses of the planes.
 * For each plane, NvBufferMemMap is called which gets the
 * memory-mapped virtual address of the plane with the access
 * pointed by the flag in the void data-pointer.
 * Before the mapped memory is accessed, a call to NvBufferMemSyncForCpu()
 * with the virtual address must be present, before any access is made
 * by the CPU to the buffer.
 *
 * After reading the data, the memory-mapped virtual address of the
 * plane is unmapped.
**/
static int
write_video_frame(int dest_dma_fd, ofstream * output_stream)
{
    int ret = 0;
    NvBufferParams dest_param;
    ret = NvBufferGetParams (dest_dma_fd, &dest_param );
    if (ret)
    {
        cerr << "Get Params failed" << endl;
        return -1;
    }

    /* Void data pointer to store memory-mapped
    ** virtual addresses of the planes.
    */

    void *virtualop_data_addr;
    unsigned int plane = 0;

    for ( plane = 0; plane < dest_param.num_planes ; ++plane)
    {
        ret = NvBufferMemMap (dest_dma_fd, plane, NvBufferMem_Read_Write, &virtualop_data_addr);
        if (ret == 0)
        {
            unsigned int i = 0;

            /* Syncs HW memory for reading from
            ** the buffer.
            */

            NvBufferMemSyncForCpu (dest_dma_fd, plane, &virtualop_data_addr);
            for (i = 0; i < dest_param.height[plane]; ++i)
            {
                streamsize bytes_to_write = dest_param.width[plane] * bytes_per_pixel_destfmt[plane];
                output_stream->write ((char*)virtualop_data_addr + i * dest_param.pitch[plane],
                    bytes_to_write);
                if (!output_stream->good())
                {
                    cerr << "File write failure" << endl;
                    return -1;
                }
            }

        }
        NvBufferMemUnMap (dest_dma_fd, plane, &virtualop_data_addr);
    }
    return 0;
}

int
main(int argc, char const *argv[])
{
    ifstream *input_file = NULL;
    string input_file_path;
    ofstream *output_file = NULL;
    string output_file_path;
    int ret = 0;
    bool in_error = 0;

    // Initialisation.
    int width = 0;
    int height = 0;
    int source_dmabuf_fd = -1;
    int dest_dmabuf_fd = -1;
    bool eos = false;

    NvBufferCreateParams input_params = {0};
    NvBufferCreateParams output_params = {0};
    NvBufferColorFormat input_color_format = NvBufferColorFormat_Invalid;
    NvBufferColorFormat output_color_format = NvBufferColorFormat_Invalid;
    NvBufferTransformParams transform_params;
    NvBufferRect src_rect, dest_rect;

    if (!strcmp(argv[1], "-h") || !strcmp(argv[1],"--help"))
    {
        print_help();
        return 0;
    }

    assert (argc == 7);
    input_file_path = argv[1];
    get_color_format(argv[2], &input_color_format);
    width = atoi(argv[3]);
    height = atoi(argv[4]);
    output_file_path = argv[5];
    get_color_format(argv[6], &output_color_format);

    if (width <= 0 || height  <= 0)
    {
       cerr << "Width and Height should be positive integers" << endl;
       return -1;
    }

    if (input_color_format == NvBufferColorFormat_Invalid ||
        output_color_format == NvBufferColorFormat_Invalid)
    {
        cerr << "Error, invalid input or output pixel format" << endl;
        print_help();
        return -1;
    }

    // I/O file operations.

    input_file = new ifstream(input_file_path);
    CHECK_ERROR(!input_file->is_open(),
        "Error in opening input file", cleanup);

    output_file = new ofstream(output_file_path);
    CHECK_ERROR(!output_file->is_open(),
        "Error in opening output file", cleanup);


    /* Define the parameter for the HW Buffer.
    ** @payloadType defines the memory handle
    ** for the NvBuffer, here defined for the
    ** set of planese.
    ** @nvbuf_tag identifies the type of device
    ** or compoenet requesting the operation.
    ** @layout defines memory layout for the
    ** surfaces, either Pitch/BLockLinear
    ** (Note: The BlockLinear surfaces allocated
    ** needs to be again transformed to Pitch
    ** for dumping the buffer).
    */

    input_params.width = width;
    input_params.height = height;
    input_params.layout = NvBufferLayout_Pitch;
    input_params.payloadType = NvBufferPayload_SurfArray;
    input_params.colorFormat = input_color_format;
    input_params.nvbuf_tag = NvBufferTag_VIDEO_CONVERT;

    output_params.width = width;
    output_params.height = height;
    output_params.layout = NvBufferLayout_Pitch;
    output_params.payloadType = NvBufferPayload_SurfArray;
    output_params.nvbuf_tag = NvBufferTag_VIDEO_CONVERT;
    output_params.colorFormat = output_color_format;

    /* Store th bpp required for each color
    ** format to read/write properly to raw
    ** buffers.
    */

    fill_bytes_per_pixel(input_params.colorFormat, bytes_per_pixel_srcfmt);
    fill_bytes_per_pixel(output_params.colorFormat, bytes_per_pixel_destfmt);

    /* Create the HW Buffer. It is exported as
    ** an FD by the hardware.
    */

    ret = NvBufferCreateEx(&source_dmabuf_fd, &input_params);
    CHECK_ERROR(ret,
        "Error in creating the source buffer.", cleanup);

    ret = NvBufferCreateEx(&dest_dmabuf_fd, &output_params);
    CHECK_ERROR(ret,
        "Error in creating the destination buffer.", cleanup);

    /* Transformation parameters are now defined
    ** which is passed to the NvBuuferTransform
    ** for required conversion.
    */

    src_rect.top = 0;
    src_rect.left = 0;
    src_rect.width = width;
    src_rect.height = height;
    dest_rect.top = 0;
    dest_rect.left = 0;
    dest_rect.width = width;
    dest_rect.height = height;

    /* @transform_flag defines the flags for
    ** enabling the valid transforms.
    ** All the valid parameters are present in
    ** the nvbuf_utils header.
    */

    memset(&transform_params,0,sizeof(transform_params));
    transform_params.transform_flag = NVBUFFER_TRANSFORM_FILTER;
    transform_params.transform_filter = NvBufferTransform_Filter_Nearest;
    transform_params.src_rect = src_rect;
    transform_params.dst_rect = dest_rect;

    /* The main loop for reading the data from
    ** file into the HW source buffer, calling
    ** the transform and writing the output
    ** bytestream back to the destination file.
    */

    while (!eos)
    {
        if (read_video_frame(source_dmabuf_fd, input_file) < 0)
        {
            cout << "File read complete." << endl;
            eos = true;
            break;
        }

        ret = NvBufferTransform(source_dmabuf_fd, dest_dmabuf_fd, &transform_params);
        CHECK_ERROR(ret, "Error in transformation.", cleanup);

        ret = write_video_frame(dest_dmabuf_fd, output_file);
        CHECK_ERROR(ret,
        "Error in dumping the output raw buffer.", cleanup);

    }

cleanup:
    if (input_file->is_open())
    {
        delete input_file;
    }
    if (output_file->is_open())
    {
        delete output_file;
    }

    /* HW allocated buffers must be destroyed
    ** at the end of execution.
    */

    if (source_dmabuf_fd != -1)
    {
        NvBufferDestroy(source_dmabuf_fd);
    }

    if (dest_dmabuf_fd != -1)
    {
        NvBufferDestroy(dest_dmabuf_fd);
    }

    if (in_error)
    {
        cerr << "Transform Failed" << endl;
    }
    else
    {
        cout << "Transform Successful" << endl;
    }

    return ret;
}
