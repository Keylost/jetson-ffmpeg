/*
 * Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <cuda_runtime.h>
#include "yuv2rgb.cuh"

__device__ inline float clamp(float val, float mn, float mx)
{
	return (val >= mn)? ((val <= mx)? val : mx) : mn;
}

__global__ void gpuConvertYUYVtoRGB_kernel(unsigned char *src, unsigned char *dst,
		unsigned int width, unsigned int height)
{
	int idx = blockIdx.x * blockDim.x + threadIdx.x;
	if (idx*2 >= width) {
		return;
	}

	for (int i = 0; i < height; ++i) {
		int y0 = src[i*width*2+idx*4+0];
		int cb = src[i*width*2+idx*4+1];
		int y1 = src[i*width*2+idx*4+2];
		int cr = src[i*width*2+idx*4+3];

		dst[i*width*3+idx*6+0] = clamp(1.164f * (y0 - 16) + 1.596f * (cr - 128)                      , 0.0f, 255.0f);
		dst[i*width*3+idx*6+1] = clamp(1.164f * (y0 - 16) - 0.813f * (cr - 128) - 0.391f * (cb - 128), 0.0f, 255.0f);
		dst[i*width*3+idx*6+2] = clamp(1.164f * (y0 - 16)                       + 2.018f * (cb - 128), 0.0f, 255.0f);

		dst[i*width*3+idx*6+3] = clamp(1.164f * (y1 - 16) + 1.596f * (cr - 128)                      , 0.0f, 255.0f);
		dst[i*width*3+idx*6+4] = clamp(1.164f * (y1 - 16) - 0.813f * (cr - 128) - 0.391f * (cb - 128), 0.0f, 255.0f);
		dst[i*width*3+idx*6+5] = clamp(1.164f * (y1 - 16)                       + 2.018f * (cb - 128), 0.0f, 255.0f);
	}
}

void gpuConvertYUYVtoRGB(unsigned char *src, unsigned char *dst,
		unsigned int width, unsigned int height)
{
	unsigned char *d_src = NULL;
	unsigned char *d_dst = NULL;
	size_t planeSize = width * height * sizeof(unsigned char);

	unsigned int flags;
	bool srcIsMapped = (cudaHostGetFlags(&flags, src) == cudaSuccess) && (flags & cudaHostAllocMapped);
	bool dstIsMapped = (cudaHostGetFlags(&flags, dst) == cudaSuccess) && (flags & cudaHostAllocMapped);

	if (srcIsMapped) {
		d_src = src;
		cudaStreamAttachMemAsync(NULL, src, 0, cudaMemAttachGlobal);
	} else {
		cudaMalloc(&d_src, planeSize * 2);
		cudaMemcpy(d_src, src, planeSize * 2, cudaMemcpyHostToDevice);
	}
	if (dstIsMapped) {
		d_dst = dst;
		cudaStreamAttachMemAsync(NULL, dst, 0, cudaMemAttachGlobal);
	} else {
		cudaMalloc(&d_dst, planeSize * 3);
	}

	unsigned int blockSize = 1024;
	unsigned int numBlocks = (width / 2 + blockSize - 1) / blockSize;
	gpuConvertYUYVtoRGB_kernel<<<numBlocks, blockSize>>>(d_src, d_dst, width, height);
	cudaStreamAttachMemAsync(NULL, dst, 0, cudaMemAttachHost);
	cudaStreamSynchronize(NULL);

	if (!srcIsMapped) {
		cudaMemcpy(dst, d_dst, planeSize * 3, cudaMemcpyDeviceToHost);
		cudaFree(d_src);
	}
	if (!dstIsMapped) {
		cudaFree(d_dst);
	}
}
