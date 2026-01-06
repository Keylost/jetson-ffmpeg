/*
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
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

#ifndef CUDA_BAYER_DEMOSAIC_CONSUMER_H
#define CUDA_BAYER_DEMOSAIC_CONSUMER_H

#include "CUDAHelper.h"
#include "Error.h"
#include "PreviewConsumer.h"

#include "CudaBayerDemosaicKernel.h"

namespace ArgusSamples
{

/**
 * The CudaBayerDemosaicConsumer acts as an EGLStream consumer for Bayer buffers output
 * from argus (RAW16), and uses a CUDA kernel to perform simple Bayer demosaic on the
 * input in order to output RGBA data. This component then acts as a producer to another
 * EGLStream, pushing the RGBA results buffer into a PreviewConsumer so that its
 * contents are rendered on screen using OpenGL.
 *
 * This sample effectively chains two EGLStreams together as follows:
 *   Argus --> [Bayer EGLStream] --> CUDA Demosaic --> [RGBA EGLStream] --> OpenGL
 */
class CudaBayerDemosaicConsumer : public Thread
{
public:

    explicit CudaBayerDemosaicConsumer(EGLDisplay display, EGLStreamKHR stream,
                                       Argus::Size2D<uint32_t> size, uint32_t frameCount);
    ~CudaBayerDemosaicConsumer();

private:
    /** @name Thread methods */
    /**@{*/
    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();
    /**@}*/

    static const uint32_t RGBA_BUFFER_COUNT = 2; // Number of buffers to alloc in RGBA stream.

    EGLDisplay m_eglDisplay;            // EGLDisplay handle.
    EGLStreamKHR m_bayerInputStream;    // EGLStream between Argus and CUDA.
    EGLStreamHolder m_rgbaOutputStream; // EGLStream between CUDA and OpenGL.

    Argus::Size2D<uint32_t> m_bayerSize;  // Size of Bayer input.
    Argus::Size2D<uint32_t> m_outputSize; // Size of RGBA output.

    uint32_t m_frameCount;              // Number of frames to process.

    CUcontext m_cudaContext;
    CUeglStreamConnection m_cudaBayerStreamConnection; // CUDA handle to Bayer stream.
    CUeglStreamConnection m_cudaRGBAStreamConnection;  // CUDA handle to RGBA stream.

    CUdeviceptr m_rgbaBuffers[RGBA_BUFFER_COUNT]; // RGBA buffers used for CUDA output.

    PreviewConsumerThread* m_previewConsumerThread; // OpenGL consumer thread.
};

}; // namespace ArgusSamples

#endif // CUDA_BAYER_DEMOSAIC_CONSUMER_H
