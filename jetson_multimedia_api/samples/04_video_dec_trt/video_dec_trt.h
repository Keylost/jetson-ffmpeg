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

#include "NvVideoDecoder.h"
#include "NvVideoConverter.h"
#include <queue>
#include <map>
#include <fstream>
#include <pthread.h>
#include "trt_inference.h"
#include "nvbuf_utils.h"
#include <cuda_runtime_api.h>
#include <cuda.h>
#include <cuda_runtime.h>
#include "cudaEGL.h"

using namespace std;

#define MAX_CHANNEL 32

typedef struct
{
    uint32_t window_height;
    uint32_t window_width;
    EGLDisplay egl_display;
}AppDisplayContext;


typedef struct
{
    NvVideoDecoder *dec;
    uint32_t decoder_pixfmt;

    char *in_file_path;
    std::ifstream *in_file;

    bool disable_dpb;
    uint32_t dec_width;
    uint32_t dec_height;

    std::queue < int > *dec_output_empty_queue;
    pthread_mutex_t empty_queue_lock;
    pthread_cond_t empty_queue_cond;

    std::queue < int > *dec_output_filled_queue;
    pthread_mutex_t filled_queue_lock;
    pthread_cond_t filled_queue_cond;

    pthread_t dec_capture_loop;
    pthread_t dec_output_loop;
    bool got_error;
    bool got_eos;
    int dma_buf_num;
    int* dst_dma_fd;
    int network_width;
    int network_height;
    int thread_id;
    char output_path[256];
    CUeglFrame* eglFramePtr;
    CUgraphicsResource* pResource;
    cudaStream_t* pStream_conversion;
    EGLImageKHR* egl_imagePtr;
    map<int, CUeglFrame> dma_egl_map;
    ofstream fstream;
    AppDisplayContext *display_context;
} AppDecContext;



typedef struct
{
    string deployfile;
    string modelfile;
    string onnxmodelfile;
    TRT_Context          *trt_ctx;
    int                   trt_stop;
    pthread_t             trt_thread_handle;
    int                  dec_num;
    int                  bLastframe[MAX_CHANNEL];
    AppDecContext        *dec_context[MAX_CHANNEL];
    AppDisplayContext    *display_context;
} AppTRTContext;

class TRT_Context;

int parseCsvArgs(AppDecContext *ctx, AppTRTContext *trt_ctx, int argc, char *argv[]);
