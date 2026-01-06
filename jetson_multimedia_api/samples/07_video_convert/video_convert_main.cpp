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

#include <fstream>
#include <iostream>
#include <vector>
#include <cassert>
#include <string.h>
#include <sys/time.h>

#include "video_convert.h"

using namespace std;

#define PERF_LOOP   3000

struct thread_context
{
    ifstream *in_file;
    ofstream *out_file;
    int in_dmabuf_fd;
    int out_dmabuf_fd;
    NvBufferCreateParams input_params;
    NvBufferCreateParams output_params;
    NvBufferTransformParams transform_params;
    vector<int> src_fmt_bytes_per_pixel;
    vector<int> dest_fmt_bytes_per_pixel;
    NvBufferSyncObj syncobj;
    bool perf;
    bool async;
};

/**
 * This function returns vector contians bytes per pixel info
 * of each plane in sequence.
**/
static int
fill_bytes_per_pixel(NvBufferColorFormat pixel_format, vector<int> *bytes_per_pixel_fmt)
{
    switch (pixel_format)
    {
        case NvBufferColorFormat_NV12:
        case NvBufferColorFormat_NV12_ER:
        case NvBufferColorFormat_NV21:
        case NvBufferColorFormat_NV21_ER:
        case NvBufferColorFormat_NV12_709:
        case NvBufferColorFormat_NV12_709_ER:
        case NvBufferColorFormat_NV12_2020:
        case NvBufferColorFormat_NV16:
        case NvBufferColorFormat_NV24:
        case NvBufferColorFormat_NV16_ER:
        case NvBufferColorFormat_NV24_ER:
        case NvBufferColorFormat_NV16_709:
        case NvBufferColorFormat_NV24_709:
        case NvBufferColorFormat_NV16_709_ER:
        case NvBufferColorFormat_NV24_709_ER:
        {
            bytes_per_pixel_fmt->push_back(1);
            bytes_per_pixel_fmt->push_back(2);
            break;
        }
        case NvBufferColorFormat_NV12_10LE:
        case NvBufferColorFormat_NV12_10LE_709:
        case NvBufferColorFormat_NV12_10LE_709_ER:
        case NvBufferColorFormat_NV12_10LE_2020:
        case NvBufferColorFormat_NV21_10LE:
        case NvBufferColorFormat_NV12_12LE:
        case NvBufferColorFormat_NV12_12LE_2020:
        case NvBufferColorFormat_NV21_12LE:
        case NvBufferColorFormat_NV16_10LE:
        case NvBufferColorFormat_NV24_10LE_709:
        case NvBufferColorFormat_NV24_10LE_709_ER:
        case NvBufferColorFormat_NV24_10LE_2020:
        case NvBufferColorFormat_NV24_12LE_2020:
        {
            bytes_per_pixel_fmt->push_back(2);
            bytes_per_pixel_fmt->push_back(4);
            break;
        }
        case NvBufferColorFormat_ABGR32:
        case NvBufferColorFormat_XRGB32:
        case NvBufferColorFormat_ARGB32:
        {
            bytes_per_pixel_fmt->push_back(4);
            break;
        }
        case NvBufferColorFormat_YUV420:
        case NvBufferColorFormat_YUV420_ER:
        case NvBufferColorFormat_YUV420_709:
        case NvBufferColorFormat_YUV420_709_ER:
        case NvBufferColorFormat_YUV420_2020:
        case NvBufferColorFormat_YUV444:
        {
            bytes_per_pixel_fmt->push_back(1);
            bytes_per_pixel_fmt->push_back(1);
            bytes_per_pixel_fmt->push_back(1);
            break;
        }
        case NvBufferColorFormat_UYVY:
        case NvBufferColorFormat_UYVY_ER:
        case NvBufferColorFormat_VYUY:
        case NvBufferColorFormat_VYUY_ER:
        case NvBufferColorFormat_YUYV:
        case NvBufferColorFormat_YUYV_ER:
        case NvBufferColorFormat_YVYU:
        case NvBufferColorFormat_YVYU_ER:
        {
            bytes_per_pixel_fmt->push_back(2);
            break;
        }
        case NvBufferColorFormat_GRAY8:
        {
            bytes_per_pixel_fmt->push_back(1);
            break;
        }
        default:
            return -1;
    }
    return 0;
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
read_video_frame(int src_dma_fd, ifstream * input_stream, const vector<int> &bytes_per_pixel_fmt)
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

    assert(src_param.num_planes == bytes_per_pixel_fmt.size());
    for (plane = 0; plane < src_param.num_planes ; ++plane)
    {
        ret = NvBufferMemMap (src_dma_fd, plane, NvBufferMem_Write, &virtualip_data_addr);
        if (ret == 0)
        {
            unsigned int i = 0;
            for (i = 0; i < src_param.height[plane]; ++i)
            {
                streamsize bytes_to_read = src_param.width[plane] * bytes_per_pixel_fmt[plane];
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
write_video_frame(int dest_dma_fd, ofstream * output_stream, const vector<int> &bytes_per_pixel_fmt)
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

    assert(dest_param.num_planes == bytes_per_pixel_fmt.size());
    for ( plane = 0; plane < dest_param.num_planes; ++plane)
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
                streamsize bytes_to_write = dest_param.width[plane] * bytes_per_pixel_fmt[plane];
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

static int
create_thread_context(context_t *ctx, struct thread_context *tctx, int index)
{
    int ret = 0;
    string out_file_path(ctx->out_file_path);

    tctx->in_file = new ifstream(ctx->in_file_path);
    if (!tctx->in_file->is_open())
    {
        cerr << "Could not open input file" << endl;
        goto out;
    }
    tctx->out_file = new ofstream(out_file_path + to_string(index));
    if (!tctx->out_file->is_open())
    {
        cerr << "Could not open output file" << endl;
        goto out;
    }

    /* Define the parameter for the HW Buffer.
    ** @payloadType: Define the memory handle for the NvBuffer,
    **               here defined for the set of planese.
    ** @nvbuf_tag: Identifie the type of device or compoenet
    **             requesting the operation.
    ** @layout: Defines memory layout for the surfaces, either
    **          NvBufferLayout_Pitch or NvBufferLayout_BlockLinear.
    ** (Note: This sample needs to read data from file and
    **        dump converted buffer to file, so input and output
    **        layout are both NvBufferLayout_Pitch.
    */
    tctx->input_params.width = ctx->in_width;
    tctx->input_params.height = ctx->in_height;
    tctx->input_params.layout = NvBufferLayout_Pitch;
    tctx->input_params.payloadType = NvBufferPayload_SurfArray;
    tctx->input_params.colorFormat = ctx->in_pixfmt;
    tctx->input_params.nvbuf_tag = NvBufferTag_VIDEO_CONVERT;

    tctx->output_params.width = ctx->out_width;
    tctx->output_params.height = ctx->out_height;
    tctx->output_params.layout = NvBufferLayout_Pitch;
    tctx->output_params.payloadType = NvBufferPayload_SurfArray;
    tctx->output_params.nvbuf_tag = NvBufferTag_VIDEO_CONVERT;
    tctx->output_params.colorFormat = ctx->out_pixfmt;

    /* Create the HW Buffer. It is exported as
    ** an FD by the hardware.
    */
    tctx->in_dmabuf_fd = -1;
    ret = NvBufferCreateEx(&tctx->in_dmabuf_fd, &tctx->input_params);
    if (ret)
    {
        cerr << "Error in creating the input buffer." << endl;
        goto out;
    }

    tctx->out_dmabuf_fd = -1;
    ret = NvBufferCreateEx(&tctx->out_dmabuf_fd, &tctx->output_params);
    if (ret)
    {
        cerr << "Error in creating the output buffer." << endl;
        goto out;
    }

    /* Store th bpp required for each color
    ** format to read/write properly to raw
    ** buffers.
    */
    ret = fill_bytes_per_pixel(ctx->in_pixfmt, &tctx->src_fmt_bytes_per_pixel);
    if (ret)
    {
        cerr << "Error figure out bytes per pixel for source format." << endl;
        goto out;
    }
    ret = fill_bytes_per_pixel(ctx->out_pixfmt, &tctx->dest_fmt_bytes_per_pixel);
    if (ret)
    {
        cerr << "Error figure out bytes per pixel for destination format." << endl;
        goto out;
    }

    /* @transform_flag defines the flags for
    ** enabling the valid transforms.
    ** All the valid parameters are present in
    ** the nvbuf_utils header.
    */
    memset(&tctx->transform_params, 0, sizeof(tctx->transform_params));
    tctx->transform_params.transform_flag = NVBUFFER_TRANSFORM_FILTER | NVBUFFER_TRANSFORM_FLIP;
    if (ctx->crop_rect.width != 0 && ctx->crop_rect.height != 0)
    {
        tctx->transform_params.transform_flag |= NVBUFFER_TRANSFORM_CROP_SRC;
        tctx->transform_params.src_rect = ctx->crop_rect;
    }
    tctx->transform_params.transform_flip = ctx->flip_method;
    tctx->transform_params.transform_filter = ctx->interpolation_method;
    if (ctx->create_session)
    {
        tctx->transform_params.session = NvBufferSessionCreate();
    }

    tctx->perf = ctx->perf;
    tctx->async = ctx->async;

out:
    return ret;
}

static void
destory_thread_context(context_t *ctx, struct thread_context *tctx)
{
    if (tctx->in_file && tctx->in_file->is_open())
    {
        delete tctx->in_file;
    }
    if (tctx->out_file && tctx->out_file->is_open())
    {
        delete tctx->out_file;
    }

    /* HW allocated buffers must be destroyed
    ** at the end of execution.
    */
    if (tctx->in_dmabuf_fd != -1)
    {
        NvBufferDestroy(tctx->in_dmabuf_fd);
    }

    if (tctx->out_dmabuf_fd != -1)
    {
        NvBufferDestroy(tctx->out_dmabuf_fd);
    }
    if (ctx->create_session && tctx->transform_params.session)
    {
        NvBufferSessionDestroy(tctx->transform_params.session);
    }
}

static void *
do_video_convert(void *arg)
{
    struct thread_context *tctx = (struct thread_context *)arg;
    int ret = 0;
    int count = tctx->perf ? PERF_LOOP : 1;

    /* The main loop for reading the data from
    ** file into the HW source buffer, calling
    ** the transform and writing the output
    ** bytestream back to the destination file.
    */
    while (true)
    {
        ret = read_video_frame(tctx->in_dmabuf_fd, tctx->in_file, tctx->src_fmt_bytes_per_pixel);
        if (ret < 0)
        {
            cout << "File read complete." << endl;
            break;
        }
        if (!tctx->async)
        {
            for (int i = 0; i < count; ++i)
            {
                ret = NvBufferTransform(tctx->in_dmabuf_fd, tctx->out_dmabuf_fd, &tctx->transform_params);
                if (ret)
                {
                    cerr << "Error in transformation." << endl;
                    goto out;
                }
            }
        }
        else
        {
            memset(&tctx->syncobj,0,sizeof(NvBufferSyncObj));
            tctx->syncobj.use_outsyncobj = 1;

            for (int i = 0; i < count; ++i)
            {
                ret = NvBufferTransformAsync(tctx->in_dmabuf_fd, tctx->out_dmabuf_fd,
                            &tctx->transform_params, &tctx->syncobj);
                if (ret)
                {
                    cerr << "Error in asynchronous transformation." << endl;
                    goto out;
                }

                ret =  NvBufferSyncObjWait(&tctx->syncobj.outsyncobj,
                            NVBUFFER_SYNCPOINT_WAIT_INFINITE);
                if (ret)
                {
                    cerr << "Error in sync object wait." << endl;
                    goto out;
                }
            }
        }
        ret = write_video_frame(tctx->out_dmabuf_fd, tctx->out_file, tctx->dest_fmt_bytes_per_pixel);
        if (ret)
        {
            cerr << "Error in dumping the output raw buffer." << endl;
            break;
        }
    }

out:
    return nullptr;
}

static void
set_defaults(context_t * ctx)
{
    memset(ctx, 0, sizeof(context_t));

    ctx->num_thread = 1;
    ctx->async = false;
    ctx->create_session = false;
    ctx->perf = false;
    ctx->flip_method = NvBufferTransform_None;
    ctx->interpolation_method = NvBufferTransform_Filter_Nearest;
}

int
main(int argc, char *argv[])
{
    context_t ctx;
    struct thread_context tctx;
    int ret = 0;
    pthread_t *tids = nullptr;
    struct thread_context *thread_ctxs = nullptr;
    struct timeval start_time;
    struct timeval stop_time;

    set_defaults(&ctx);

    ret = parse_csv_args(&ctx, argc, argv);
    if (ret < 0)
    {
        cerr << "Error parsing commandline arguments" << endl;
        goto cleanup;
    }

    tids = new pthread_t[ctx.num_thread];
    thread_ctxs = new struct thread_context[ctx.num_thread];

    for (uint32_t i = 0; i < ctx.num_thread; ++i)
    {
        ret = create_thread_context(&ctx, &thread_ctxs[i], i);
        if (ret)
        {
            cerr << "Error when init thread context " << i << endl;
            goto cleanup;
        }
    }

    if (ctx.perf)
    {
        gettimeofday(&start_time, nullptr);
    }

    for (uint32_t i = 0; i < ctx.num_thread; ++i)
    {
        pthread_create(&tids[i], nullptr, do_video_convert, &thread_ctxs[i]);
    }

    pthread_yield();

    for (uint32_t i = 0; i < ctx.num_thread; ++i)
    {
        pthread_join(tids[i], nullptr);
    }

    if (ctx.perf)
    {
        unsigned long total_time_us = 0;

        gettimeofday(&stop_time, nullptr);
        total_time_us = (stop_time.tv_sec - start_time.tv_sec) * 1000000 +
                   (stop_time.tv_usec - start_time.tv_usec);

        cout << endl;
        cout << "Total conversion takes " << total_time_us << " us, average "
             << total_time_us / PERF_LOOP / ctx.num_thread << " us per conversion" << endl;
        cout << endl;
    }

cleanup:

    for (uint32_t i = 0; i < ctx.num_thread; ++i)
    {
        destory_thread_context(&ctx, &thread_ctxs[i]);
    }

    free(ctx.in_file_path);
    free(ctx.out_file_path);

    delete []tids;
    delete []thread_ctxs;

    if (ret)
    {
        cout << "App run failed" << endl;
    }
    else
    {
        cout << "App run was successful" << endl;
    }

    return ret;
}
