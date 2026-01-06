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
#ifndef TRT_INFERENCE_H_
#define TRT_INFERENCE_H_

#include <fstream>
#include <queue>
#include "NvInfer.h"
#include "NvCaffeParser.h"
#include "NvOnnxParser.h"
#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <opencv2/objdetect/objdetect.hpp>
using namespace nvinfer1;
using namespace nvcaffeparser1;
using namespace nvonnxparser;
using namespace std;

// Model Index
#define GOOGLENET_SINGLE_CLASS 0
#define GOOGLENET_THREE_CLASS  1
#define RESNET_THREE_CLASS  2

class Logger;

class Profiler;

class TRT_Context
{
public:
    //net related parameter
    int getNetWidth() const;

    int getNetHeight() const;

    uint32_t getBatchSize() const;

    int getChannel() const;

    int getModelClassCnt() const;

    void* getScales() const;

    void* getOffsets() const;

    // Buffer is allocated in TRT_Conxtex,
    // Expose this interface for inputing data
    void*& getBuffer(const int& index);

    float*& getInputBuf();

    uint32_t getNumTrtInstances() const;

    //0 fp16  1 fp32  2 int8
    void setMode(const int& mode);

    void setBatchSize(const uint32_t& batchsize);

    void setDumpResult(const bool& dump_result);

    void setTrtProfilerEnabled(const bool& enable_trt_profiler);

    int getFilterNum() const;
    void setFilterNum(const unsigned int& filter_num);

    TRT_Context();

    void setModelIndex(int modelIndex);

    void buildTrtContext(const string& deployfile,
            const string& modelfile, bool bUseCPUBuf = false, bool isOnnxModel = false);

    void doInference(
        queue< vector<cv::Rect> >* rectList_queue,
        float *input = NULL);

    void destroyTrtContext(bool bUseCPUBuf = false);

    ~TRT_Context();

private:
    int net_width;
    int net_height;
    int filter_num;
    void  **buffers;
    float *input_buf;
    float *output_cov_buf;
    float *output_bbox_buf;
    void* offset_gpu;
    void* scales_gpu;
    float helnet_scale[4];
    IRuntime *runtime;
    ICudaEngine *engine;
    IExecutionContext *context;
    uint32_t *pResultArray;
    int channel;              //input file's channel
    int num_bindings;
    int trtinstance_num;      //inference channel num
    int batch_size;
    int mode;
    bool dump_result;
    ofstream fstream;
    bool enable_trt_profiler;
    bool is_onnx_model;
    IHostMemory *trtModelStream{nullptr};
    vector<string> outputs;
    string result_file;
    Logger *pLogger;
    Profiler *pProfiler;
    int frame_num;
    uint64_t elapsed_frame_num;
    uint64_t elapsed_time;
    int inputIndex;
    int outputIndex;
    int outputIndexBBOX;
    Dims3 inputDims;
    Dims3 outputDims;
    Dims3 outputDimsBBOX;
    size_t inputSize;
    size_t outputSize;
    size_t outputSizeBBOX;

    struct {
        const int  classCnt;
        float      THRESHOLD[3];
        const char *INPUT_BLOB_NAME;
        const char *OUTPUT_BLOB_NAME;
        const char *OUTPUT_BBOX_NAME;
        const int  STRIDE;
        const int  WORKSPACE_SIZE;
        int        offsets[3];
        float      input_scale[3];
        float      bbox_output_scales[4];
        const int  ParseFunc_ID;
    } *g_pModelNetAttr, gModelNetAttr[4] = {
        {
            // GOOGLENET_SINGLE_CLASS
            1,
            {0.8, 0, 0},
            "data",
            "coverage",
            "bboxes",
            4,
            450 * 1024 * 1024,
            {0, 0, 0},
            {1.0f, 1.0f, 1.0f},
            {1, 1, 1, 1},
            0
        },

        {
            // GOOGLENET_THREE_CLASS
            3,
            {0.6, 0.6, 1.0},   //People, Motorbike, Car
            "data",
            "Layer16_cov",
            "Layer16_bbox",
            16,
            110 * 1024 * 1024,
            {124, 117, 104},
            {1.0f, 1.0f, 1.0f},
            {-640, -368, 640, 368},
            0
        },

        {
            // RESNET_THREE_CLASS
            4,
            {0.1, 0.1, 0.1},   //People, Motorbike, Car
            "data",
            "Layer7_cov",
            "Layer7_bbox",
            16,
            110 * 1024 * 1024,
            {0, 0, 0},
            {0.0039215697906911373, 0.0039215697906911373, 0.0039215697906911373},
            {-640, -368, 640, 368},
            1
        },
    };
    enum Mode_type{
        MODE_FP16 = 0,
        MODE_FP32 = 1,
        MODE_INT8 = 2
    };
    int parseNet(const string& deployfile);
    void parseBbox(vector<cv::Rect>* rectList, int batch_th);
    void ParseResnet10Bbox(vector<cv::Rect>* rectList, int batch_th);
    void allocateMemory(bool bUseCPUBuf);
    void releaseMemory(bool bUseCPUBuf);
    void caffeToTRTModel(const string& deployfile, const string& modelfile);
    void onnxToTRTModel(const string& modelfile);
};

#endif
