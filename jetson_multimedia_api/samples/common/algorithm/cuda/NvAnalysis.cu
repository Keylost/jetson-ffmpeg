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

#include <cuda.h>
#include "NvAnalysis.h"

#define BOX_W 32
#define BOX_H 32

__global__ void
addLabelsKernel(int *pDevPtr, int pitch)
{
    int row = blockIdx.y * blockDim.y + threadIdx.y + BOX_H;
    int col = blockIdx.x * blockDim.x + threadIdx.x + BOX_W;
    char *pElement = (char *)pDevPtr + row * pitch + col;

    pElement[0] = 0;

    return;
}

int
addLabels(CUdeviceptr pDevPtr, int pitch)
{
    dim3 threadsPerBlock(BOX_W, BOX_H);
    dim3 blocks(1,1);

    addLabelsKernel<<<blocks,threadsPerBlock>>>((int *)pDevPtr, pitch);

    return 0;
}


__global__ void
convertIntToFloatKernelRGB(CUdeviceptr pDevPtr, int width, int height,
                void* cuda_buf, int pitch, void* offsets_gpu, void* scales_gpu)
{
    float *pdata = (float *)cuda_buf;
    char *psrcdata = (char *)pDevPtr;
    int *offsets = (int *)offsets_gpu;
    float *scales = (float *)scales_gpu;
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;

    if (col < width && row < height)
    {
        for (int k = 0; k < 3; k++)
        {
            pdata[width * height * k + row * width + col] =
                (float)(*(psrcdata + row * pitch + col * 4 + (3 - 1 - k)) - offsets[k]) * scales[k];
        }
    }
}

__global__ void
convertIntToFloatKernelBGR(CUdeviceptr pDevPtr, int width, int height,
                void* cuda_buf, int pitch, void* offsets_gpu, void* scales_gpu)
{
    float *pdata = (float *)cuda_buf;
    char *psrcdata = (char *)pDevPtr;
    int *offsets = (int *)offsets_gpu;
    float *scales = (float *)scales_gpu;
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;


    if (col < width && row < height)
    {
        // For V4L2_PIX_FMT_ABGR32 --> BGRA-8-8-8-8
        for (int k = 0; k < 3; k++)
        {
            pdata[width * height * k + row * width + col] =
                (float)(*(psrcdata + row * pitch + col * 4 + k) - offsets[k]) * scales[k];
        }
    }
}

int convertIntToFloat(CUdeviceptr pDevPtr,
                      int width,
                      int height,
                      int pitch,
                      COLOR_FORMAT color_format,
                      void* offsets,
                      void* scales,
                      void* cuda_buf, void* pstream)
{
    dim3 threadsPerBlock(32, 32);
    dim3 blocks(width/threadsPerBlock.x, height/threadsPerBlock.y);
    cudaStream_t stream;
    if (pstream!= NULL)
        stream = *(cudaStream_t*)pstream;
    else
        stream = 0;

    if (color_format == COLOR_FORMAT_RGB)
    {
        convertIntToFloatKernelRGB<<<blocks, threadsPerBlock, 0, stream>>>(pDevPtr, width,
                height, cuda_buf, pitch, offsets, scales);
    }
    else if (color_format == COLOR_FORMAT_BGR)
    {
        convertIntToFloatKernelBGR<<<blocks, threadsPerBlock, 0, stream>>>(pDevPtr, width,
                height, cuda_buf, pitch, offsets, scales);
    }

    return 0;
}
