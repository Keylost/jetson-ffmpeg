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

#include "trt_inference.h"
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include <sstream>
#include <iostream>
#include <sys/stat.h>
#include <cmath>
#include <cuda_runtime_api.h>
#include <algorithm>
#include <iterator>

static const int TIMING_ITERATIONS = 1;
static const int NUM_BINDINGS = 3;
static const int FILTER_NUM = 6;

#define CHECK(status)                                   \
{                                                       \
    if (status != 0)                                    \
    {                                                   \
        std::cout << "Cuda failure: " << status;        \
        abort();                                        \
    }                                                   \
}

// Logger for TRT info/warning/errors
class Logger : public ILogger
{
    void log(Severity severity, const char* msg) noexcept override
    {
        // suppress info-level messages
        if (severity == Logger::Severity::kINTERNAL_ERROR ||
            severity == Logger::Severity::kERROR ||
            severity == Logger::Severity::kWARNING)
            std::cout << msg << std::endl;
    }
};

class Profiler : public IProfiler
{
    typedef std::pair<std::string, float> Record;
    std::vector<Record> mProfile;

    virtual void reportLayerTime(const char* layerName, float ms) noexcept
    {
        auto record = std::find_if(mProfile.begin(), mProfile.end(),
                        [&](const Record& r){ return r.first == layerName; });
        if (record == mProfile.end())
            mProfile.push_back(std::make_pair(layerName, ms));
        else
            record->second += ms;
    }

    void printLayerTimes()
    {
        float totalTime = 0;
        for (size_t i = 0; i < mProfile.size(); i++)
        {
            printf("%-40.40s %4.3fms\n", mProfile[i].first.c_str(),
                    mProfile[i].second / TIMING_ITERATIONS);
            totalTime += mProfile[i].second;
        }
        printf("Time over all layers: %4.3f\n", totalTime / TIMING_ITERATIONS);
    }

};

class Int8EntropyCalibrator : public IInt8EntropyCalibrator
{
    public:
        Int8EntropyCalibrator(bool readCache = true, bool onnxModel = false)
        {
           mReadCache = readCache;
           mOnnxModel = onnxModel;
        }

        virtual ~Int8EntropyCalibrator()
        {
        }

        //We don't support int8 calibration till now[ToDo].
        int getBatchSize() const noexcept override { return 0 /*mBF.m_Dims.n()*/; }

        bool getBatch(void* bindings[], const char* names[], int nbBindings) noexcept override
        {
            return false;
        }

        const void* readCalibrationCache(size_t& length) noexcept override
        {
            mCalibrationCache.clear();
	    const char* CACHE_PATH = mOnnxModel ? "../../data/Model/resnet10/CalibrationTable_ONNX"
		    : "../../data/Model/resnet10/CalibrationTable_CAFFE";
	    std::ifstream input(CACHE_PATH, std::ios::binary);
            input >> std::noskipws;
            if (mReadCache && input.good())
                std::copy(std::istream_iterator<char>(input), std::istream_iterator<char>(), std::back_inserter(mCalibrationCache));
            length = mCalibrationCache.size();
            cout<<"text:"<<(char*)(&mCalibrationCache[0])<<endl;
            return length ? &mCalibrationCache[0] : nullptr;
        }

        void writeCalibrationCache(const void* cache, size_t length) noexcept override
        {
            std::ofstream output("CalibrationTable", std::ios::binary);
            output.write(reinterpret_cast<const char*>(cache), length);
        }

    private:
        bool mReadCache{ true };
	bool mOnnxModel{ false };
        std::vector<char> mCalibrationCache;
};

string stringtrim(string);

//This function is used to trim space
string
stringtrim(string s)
{
    int i = 0;
    while (s[i] == ' ')
    {
        i++;
    }
    s = s.substr(i);
    i = s.size()-1;
    while (s[i] == ' ')
    {
        i--;
    }

    s = s.substr(0, i + 1);
    return s;
}

int
TRT_Context::getNetWidth() const
{
    return net_width;
}

int
TRT_Context::getNetHeight() const
{
    return net_height;
}


int
TRT_Context::getFilterNum() const
{
    return filter_num;
}

void
TRT_Context::setFilterNum(const unsigned int& filter_num)
{
    this->filter_num = filter_num;
}

void*&
TRT_Context::getBuffer(const int& index)
{
    assert(index >= 0 && index < num_bindings);
    return buffers[index];
}

float*&
TRT_Context::getInputBuf()
{
    return input_buf;
}

uint32_t
TRT_Context::getNumTrtInstances() const
{
    return trtinstance_num;
}

uint32_t
TRT_Context::getBatchSize() const
{
    return batch_size;
}

int
TRT_Context::getModelClassCnt() const
{
    return g_pModelNetAttr->classCnt;
}


void*
TRT_Context::getScales() const
{
    return scales_gpu;
}

void*
TRT_Context::getOffsets() const
{
    return offset_gpu;
}

//0 fp16  1 fp32  2 int8
void
TRT_Context::setMode(const int& mode)
{
    this->mode = mode;
}

void
TRT_Context::setBatchSize(const uint32_t& batchsize)
{
    this->batch_size = batchsize;
}

void
TRT_Context::setDumpResult(const bool& dump_result)
{
    this->dump_result = dump_result;
}

void
TRT_Context::setTrtProfilerEnabled(const bool& enable_trt_profiler)
{
    this->enable_trt_profiler = enable_trt_profiler;
}

int
TRT_Context::getChannel() const
{
    return channel;
}


TRT_Context::TRT_Context()
{
    net_width = 0;
    net_height = 0;
    filter_num = FILTER_NUM;
    buffers = new void *[NUM_BINDINGS];
    for (int i = 0; i < NUM_BINDINGS; i++)
    {
        buffers[i] = NULL;
    }
    input_buf = NULL;
    output_cov_buf = NULL;
    output_bbox_buf = NULL;

    runtime = NULL;
    engine = NULL;
    context = NULL;
    pResultArray = new uint32_t[100*4];

    channel = 0;
    num_bindings = NUM_BINDINGS;

    batch_size = 1;

    trtinstance_num = 1;

    mode = MODE_FP16;
    elapsed_frame_num = 0;
    elapsed_time = 0;
    enable_trt_profiler = 1;
    dump_result = 0;
    frame_num = 0;
    result_file = "result.txt";
    pLogger = new Logger;
    pProfiler = new Profiler;
}


void
TRT_Context::allocateMemory(bool bUseCPUBuf)
{
    const ICudaEngine& cuda_engine = context->getEngine();
    // input and output buffer pointers that we pass to the engine
    // the engine requires exactly IEngine::getNbBindings() of these
    // but in this case we know that there is exactly one input and one output
    assert(cuda_engine.getNbBindings() == num_bindings);

    // In order to bind the buffers, we need to know the names of the input
    // and output tensors. note that indices are guaranteed to be less than
    // IEngine::getNbBindings()
    inputIndex = cuda_engine.getBindingIndex(g_pModelNetAttr->INPUT_BLOB_NAME);
    outputIndex = cuda_engine.getBindingIndex(g_pModelNetAttr->OUTPUT_BLOB_NAME);
    outputIndexBBOX = cuda_engine.getBindingIndex(g_pModelNetAttr->OUTPUT_BBOX_NAME);
    // allocate GPU buffers
    if (is_onnx_model)
    {
	//It's explicit batch
        Dims inputD = cuda_engine.getBindingDimensions(inputIndex);
        Dims outputD = cuda_engine.getBindingDimensions(outputIndex);
        Dims outputDB = cuda_engine.getBindingDimensions(outputIndexBBOX);
        inputDims = Dims3{inputD.d[1], inputD.d[2], inputD.d[3]};
        outputDims = Dims3{outputD.d[1], outputD.d[2], outputD.d[3]};
        outputDimsBBOX = Dims3{outputDB.d[1], outputDB.d[2], outputDB.d[3]};
    }
    else
    {
        inputDims = static_cast<Dims3&&>(cuda_engine.getBindingDimensions(inputIndex));
        outputDims = static_cast<Dims3&&>(cuda_engine.getBindingDimensions(outputIndex));
        outputDimsBBOX = static_cast<Dims3&&>(cuda_engine.getBindingDimensions(outputIndexBBOX));
    }

    net_height = inputDims.d[1];
    net_width = inputDims.d[2];

    inputSize = batch_size * inputDims.d[0] * inputDims.d[1] * inputDims.d[2] * sizeof(float);
    outputSize = batch_size * outputDims.d[0] * outputDims.d[1] *
                            outputDims.d[2] * sizeof(float);
    outputSizeBBOX = batch_size * outputDimsBBOX.d[0] * outputDimsBBOX.d[1] *
                            outputDimsBBOX.d[2] * sizeof(float);
    if (bUseCPUBuf && input_buf == NULL)
    {
        input_buf = (float *)malloc(inputSize);
        assert(input_buf != NULL);
    }

    if (output_cov_buf == NULL)
    {
        output_cov_buf = (float *)malloc(outputSize);
        assert(output_cov_buf != NULL);
    }
    if (outputIndexBBOX >= 0)
    {
        if (output_bbox_buf == NULL)
        {
            output_bbox_buf = (float *)malloc(outputSizeBBOX);
            assert(output_bbox_buf != NULL);
        }
    }
    // create GPU buffers and a stream
    if (buffers[inputIndex] == NULL)
    {
        CHECK(cudaMalloc(&buffers[inputIndex], inputSize));
    }
    if (buffers[outputIndex] == NULL)
    {
        CHECK(cudaMalloc(&buffers[outputIndex], outputSize));
    }
    if (outputIndexBBOX >= 0)
    {
        if (buffers[outputIndexBBOX] == NULL)
        {
            CHECK(cudaMalloc(&buffers[outputIndexBBOX], outputSizeBBOX));
        }
    }
    CHECK(cudaMalloc(&offset_gpu, sizeof(int) * 3));
    CHECK(cudaMalloc(&scales_gpu, sizeof(float) * 3));
    CHECK(cudaMemcpy(offset_gpu, (void*)g_pModelNetAttr->offsets,
                                sizeof(int) * 3,
                                cudaMemcpyHostToDevice));
    CHECK(cudaMemcpy(scales_gpu, (void*)g_pModelNetAttr->input_scale,
                                sizeof(float) * 3,
                                cudaMemcpyHostToDevice));
    if (dump_result)
    {
        fstream.open(result_file.c_str(), ios::out);
    }
}

void
TRT_Context::releaseMemory(bool bUseCPUBuf)
{
    for (int i = 0; i < NUM_BINDINGS; i++)
    {
        if (buffers[i] != NULL)
        {
            CHECK(cudaFree(buffers[i]));
            buffers[i] = NULL;
        }
    }
    if (bUseCPUBuf && input_buf != NULL)
    {
        free(input_buf);
        input_buf = NULL;
    }
    if (output_cov_buf != NULL)
    {
        free(output_cov_buf);
        output_cov_buf = NULL;
    }
    if (output_bbox_buf != NULL)
    {
        free(output_bbox_buf);
        output_bbox_buf = NULL;
    }

    if (pResultArray != NULL)
    {
        delete []pResultArray;
        pResultArray = NULL;
    }

    if (dump_result)
    {
        fstream.close();
    }
    CHECK(cudaFree(offset_gpu));
    CHECK(cudaFree(scales_gpu));
}

TRT_Context::~TRT_Context()
{

    delete pLogger;
    delete pProfiler;
    delete []buffers;
}

void
TRT_Context::caffeToTRTModel(const string& deployfile, const string& modelfile)
{
    Int8EntropyCalibrator calibrator;
    IInt8Calibrator* int8Calibrator = &calibrator;
    // create API root class - must span the lifetime of the engine usage
    IBuilder *builder = createInferBuilder(*pLogger);
    INetworkDefinition *network = builder->createNetworkV2(0U);
    IBuilderConfig* config = builder->createBuilderConfig();

    // parse the caffe model to populate the network, then set the outputs
    ICaffeParser *parser = createCaffeParser();

    bool hasFp16 = builder->platformHasFastFp16();

    // if user specify
    if (mode == MODE_FP16)
    {
        if (hasFp16)
        {
            printf("mode has been set to 0(using fp16)\n");
        }
        else
        {
            printf("platform don't have fp16, force to 1(using fp32)\n");
        }
    }
    else if(mode >= MODE_FP32)
    {
        printf("mode >= 1(using fp32 or int8)\n");
        hasFp16 = 0;
    }

    // create a 16-bit model if it's natively supported
    DataType modelDataType = hasFp16 ? DataType::kHALF : DataType::kFLOAT;
    const IBlobNameToTensor *blobNameToTensor =
        parser->parse(deployfile.c_str(),    // caffe deploy file
                      modelfile.c_str(),     // caffe model file
                      *network,              // network definition that parser populate
                      modelDataType);
    assert(blobNameToTensor != nullptr);

    // the caffe file has no notion of outputs
    // so we need to manually say which tensors the engine should generate
    outputs = {g_pModelNetAttr->OUTPUT_BLOB_NAME,
               g_pModelNetAttr->OUTPUT_BBOX_NAME};
    for (auto& s : outputs)
    {
        network->markOutput(*blobNameToTensor->find(s.c_str()));
        printf("outputs %s\n", s.c_str());
    }

    // Build the engine
    builder->setMaxBatchSize(batch_size);
    config->setMaxWorkspaceSize(g_pModelNetAttr->WORKSPACE_SIZE);
    if (mode == MODE_INT8)
    {
        config->setFlag(BuilderFlag::kINT8);
        config->setInt8Calibrator(int8Calibrator);
    }

    // Eliminate the side-effect from the delay of GPU frequency boost
    config->setMinTimingIterations(3);
    config->setAvgTimingIterations(2);

    // set up the network for paired-fp16 format, only on DriveCX
    if (hasFp16)
    {
        config->setFlag(BuilderFlag::kFP16);
    }

    ICudaEngine* engine = builder->buildEngineWithConfig(*network, *config);
    assert(engine);

    // we don't need the network any more, and we can destroy the parser
    delete network;
    delete parser;
    delete config;

    // serialize the engine, then close everything down
    trtModelStream = engine->serialize();
    delete engine;
    delete builder;
    shutdownProtobufLibrary();
}

void
TRT_Context::onnxToTRTModel(const string& modelfile)
{
    Int8EntropyCalibrator calibrator(true, true);
    IInt8Calibrator* int8Calibrator = &calibrator;
    // create API root class - must span the lifetime of the engine usage
    IBuilder *builder = createInferBuilder(*pLogger);
    const auto explicitBatch = 1U << static_cast<uint32_t>(NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
    INetworkDefinition* network = builder->createNetworkV2(explicitBatch);

    IBuilderConfig* config = builder->createBuilderConfig();

    auto parser = nvonnxparser::createParser(*network, *pLogger);

    bool hasFp16 = builder->platformHasFastFp16();

    // if user specify
    if (mode == MODE_FP16)
    {
        if (hasFp16)
        {
            printf("mode has been set to 0(using fp16)\n");
        }
        else
        {
            printf("platform don't have fp16, force to 1(using fp32)\n");
        }
    }
    else if(mode >= MODE_FP32)
    {
        printf("mode >= 1(using fp32 or int8)\n");
        hasFp16 = 0;
    }

    auto parsed = parser->parseFromFile(modelfile.c_str(), static_cast<int>(ILogger::Severity::kWARNING));
    if (!parsed)
    {
        printf("Failed to parse onnx model\n");
        return;
    }

    // Only support one input for resnet10(default model)
    auto input = network->getInput(0);
    auto inputDims = input->getDimensions();
    IOptimizationProfile* profile = builder->createOptimizationProfile();
    profile->setDimensions(input->getName(), OptProfileSelector::kMIN, Dims4{batch_size, inputDims.d[1], inputDims.d[2], inputDims.d[3]});
    profile->setDimensions(input->getName(), OptProfileSelector::kOPT, Dims4{batch_size, inputDims.d[1], inputDims.d[2], inputDims.d[3]});
    profile->setDimensions(input->getName(), OptProfileSelector::kMAX, Dims4{batch_size, inputDims.d[1], inputDims.d[2], inputDims.d[3]});
    assert(config->addOptimizationProfile(profile) != -1);
    assert(profile->isValid());
    config->setCalibrationProfile(profile);

    // Build the engine
    config->setMaxWorkspaceSize(g_pModelNetAttr->WORKSPACE_SIZE);
    if (mode == MODE_INT8)
    {
        config->setFlag(BuilderFlag::kINT8);
        config->setInt8Calibrator(int8Calibrator);
    }

    // Eliminate the side-effect from the delay of GPU frequency boost
    config->setMinTimingIterations(3);
    config->setAvgTimingIterations(2);

    // set up the network for paired-fp16 format, only on DriveCX
    if (hasFp16)
    {
        config->setFlag(BuilderFlag::kFP16);
    }

    ICudaEngine* engine = builder->buildEngineWithConfig(*network, *config);
    assert(engine);

    // we don't need the network any more, and we can destroy the parser
    delete network;
    delete parser;
    delete config;


    // serialize the engine, then close everything down
    trtModelStream = engine->serialize();
    delete engine;
    delete builder;
    shutdownProtobufLibrary();
}

void
TRT_Context::setModelIndex(int index)
{
    assert(index == GOOGLENET_SINGLE_CLASS ||
           index == GOOGLENET_THREE_CLASS ||
           index == RESNET_THREE_CLASS);
    g_pModelNetAttr = gModelNetAttr + index;
    assert(g_pModelNetAttr->classCnt > 0);
    assert(g_pModelNetAttr->STRIDE > 0);
    assert(g_pModelNetAttr->WORKSPACE_SIZE > 0);
}

void
TRT_Context::buildTrtContext(const string& deployfile,
        const string& modelfile, bool bUseCPUBuf, bool isOnnxModel)
{
    this->is_onnx_model = isOnnxModel;
    if (!parseNet(deployfile) && !isOnnxModel)
    {
        cout<<"parse net failed, exit!"<<endl;
        exit(0);
    }
    ifstream trtModelFile("trtModel.cache");
    if (trtModelFile.good())
    {
        // get cache file length
        size_t size = 0;
        size_t i = 0;

        cout<<"Using cached TRT model" <<endl;

        // Get the length
        trtModelFile.seekg(0, ios::end);
        size = trtModelFile.tellg();
        trtModelFile.seekg(0, ios::beg);

        char * buff = new char [size];
        while (trtModelFile.get(buff[i])) i++;
        trtModelFile.close();
        runtime = createInferRuntime(*pLogger);
        engine = runtime->deserializeCudaEngine((void *)buff, size, nullptr);
    }
    else
    {
        if (isOnnxModel)
        {
            onnxToTRTModel(modelfile);
        } else
        {
            caffeToTRTModel(deployfile, modelfile);
        }
        cout<<"Create TRT model cache"<<endl;
        ofstream trtModelFile("trtModel.cache");
        trtModelFile.write((char *)trtModelStream->data(), trtModelStream->size());
        trtModelFile.close();
        runtime = createInferRuntime(*pLogger);
        engine = runtime->deserializeCudaEngine(trtModelStream->data(), trtModelStream->size(), nullptr);
        trtModelStream->destroy();
    }
    context = engine->createExecutionContext();
    allocateMemory(bUseCPUBuf);
}

void
TRT_Context::destroyTrtContext(bool bUseCPUBuf)
{
    releaseMemory(bUseCPUBuf);
    delete context;
    delete engine;
    delete runtime;
}

void
TRT_Context::doInference(
    queue< vector<cv::Rect> >* rectList_queue,
    float *input)
{
    struct timeval input_time;
    struct timeval output_time;

    if (!enable_trt_profiler)
    {
        cudaStream_t stream;
        CHECK(cudaStreamCreate(&stream));

        // DMA the input to the GPU,  execute the batch asynchronously
        // and DMA it back
        if (input != NULL)   //NULL means we have use GPU to map memory
        {
            CHECK(cudaMemcpyAsync(buffers[inputIndex], input, inputSize,
                                cudaMemcpyHostToDevice, stream));
        }

        if (is_onnx_model)
        {
            context->setBindingDimensions(0, Dims4{batch_size, inputDims.d[0], inputDims.d[1], inputDims.d[2]});
            context->executeV2(buffers);
        }
        else
        {
            context->enqueue(batch_size, buffers, stream, nullptr);
        }

        CHECK(cudaMemcpyAsync(output_cov_buf, buffers[outputIndex], outputSize,
                                cudaMemcpyDeviceToHost, stream));
        if (outputIndexBBOX >= 0)
        {
            CHECK(cudaMemcpyAsync(output_bbox_buf, buffers[outputIndexBBOX],
                            outputSizeBBOX, cudaMemcpyDeviceToHost, stream));
        }

        cudaStreamSynchronize(stream);

        // release the stream and the buffers
        cudaStreamDestroy(stream);
    }
    else
    {
        // DMA the input to the GPU,  execute the batch synchronously
        // and DMA it back
        if (input != NULL)   //NULL means we have use GPU to map memory
        {
            CHECK(cudaMemcpy(buffers[inputIndex], input, inputSize,
                                cudaMemcpyHostToDevice));
        }

        gettimeofday(&input_time, NULL);
        if (is_onnx_model)
	{
            context->setBindingDimensions(0, Dims4{batch_size, inputDims.d[0], inputDims.d[1], inputDims.d[2]});
            context->executeV2(buffers);
	}
        else
        {
            context->execute(batch_size, buffers);
        }
        gettimeofday(&output_time, NULL);
        CHECK(cudaMemcpy(output_cov_buf, buffers[outputIndex], outputSize,
                                cudaMemcpyDeviceToHost));
        if (outputIndexBBOX >= 0)
        {
            CHECK(cudaMemcpy(output_bbox_buf, buffers[outputIndexBBOX],
                            outputSizeBBOX, cudaMemcpyDeviceToHost));
        }
        elapsed_frame_num += batch_size;
        elapsed_time += (output_time.tv_sec - input_time.tv_sec) * 1000 +
                        (output_time.tv_usec - input_time.tv_usec) / 1000;
        if (elapsed_frame_num >= 100)
        {
            printf("Time elapsed:%ld ms per frame in past %ld frames\n",
                elapsed_time / elapsed_frame_num, elapsed_frame_num);
            elapsed_frame_num = 0;
            elapsed_time = 0;
        }
    }

    vector<cv::Rect> rectList[getModelClassCnt()];
    for (int i = 0; i < batch_size; i++)
    {
        if (g_pModelNetAttr->ParseFunc_ID == 0)
            parseBbox(rectList, i);
        else if(g_pModelNetAttr->ParseFunc_ID == 1)
            ParseResnet10Bbox(rectList, i);
        if (dump_result)
        {
            for (int class_num = 0;
                     class_num < (g_pModelNetAttr->ParseFunc_ID == 1 ? getModelClassCnt() - 1 : getModelClassCnt());
                     class_num++)
            {
                fstream << "frame:" << frame_num << " class num:" << class_num
                        << " has rect:" << rectList[class_num].size() << endl;
                for (uint32_t i = 0; i < rectList[class_num].size(); i++)
                {
                    fstream << "\tx,y,w,h:"
                            << (float) rectList[class_num][i].x / net_width << " "
                            << (float) rectList[class_num][i].y / net_height << " "
                            << (float) rectList[class_num][i].width / net_width << " "
                            << (float) rectList[class_num][i].height / net_height << endl;
                }
                fstream << endl;
            }
            frame_num++;
        }

        for (int class_num = 0; class_num < getModelClassCnt(); class_num++)
        {
            rectList_queue[class_num].push(rectList[class_num]);
        }
    }
}

void
TRT_Context::parseBbox(vector<cv::Rect>* rectList, int batch_th)
{
    int gridsize = outputDims.d[1] * outputDims.d[2];
    int gridoffset = outputDims.d[0] * outputDims.d[1] * outputDims.d[2] * batch_th;

    for (int class_num = 0; class_num < getModelClassCnt(); class_num++)
    {
        float *output_x1 = output_bbox_buf +
                outputDimsBBOX.d[0] * outputDimsBBOX.d[1] * outputDimsBBOX.d[2] * batch_th +
                class_num * 4 * outputDimsBBOX.d[1] * outputDimsBBOX.d[2];
        float *output_y1 = output_x1 + outputDimsBBOX.d[1] * outputDimsBBOX.d[2];
        float *output_x2 = output_y1 + outputDimsBBOX.d[1] * outputDimsBBOX.d[2];
        float *output_y2 = output_x2 + outputDimsBBOX.d[1] * outputDimsBBOX.d[2];

        for (int i = 0; i < gridsize; ++i)
        {
            if (output_cov_buf[gridoffset + class_num * gridsize + i] >=
                                          g_pModelNetAttr->THRESHOLD[class_num])
            {
                int g_x = i % outputDims.d[2];
                int g_y = i / outputDims.d[2];
                int i_x = g_x * g_pModelNetAttr->STRIDE;
                int i_y = g_y * g_pModelNetAttr->STRIDE;
                int rectx1 = g_pModelNetAttr->bbox_output_scales[0] * output_x1[i] + i_x;
                int recty1 = g_pModelNetAttr->bbox_output_scales[1] * output_y1[i] + i_y;
                int rectx2 = g_pModelNetAttr->bbox_output_scales[2] * output_x2[i] + i_x;
                int recty2 = g_pModelNetAttr->bbox_output_scales[3] * output_y2[i] + i_y;
                if (rectx1 < 0)
                {
                    rectx1 = 0;
                }
                if (rectx2 < 0)
                {
                    rectx2 = 0;
                }
                if (recty1 < 0)
                {
                    recty1 = 0;
                }
                if (recty2 < 0)
                {
                    recty2 = 0;
                }
                if (rectx1 >= (int)net_width)
                {
                    rectx1 = net_width - 1;
                }
                if (rectx2 >= (int)net_width)
                {
                    rectx2 = net_width - 1;
                }
                if (recty1 >= (int)net_height)
                {
                    recty1 = net_height - 1;
                }
                if (recty2 >= (int)net_height)
                {
                    recty2 = net_height - 1;
                }
                rectList[class_num].push_back(cv::Rect(rectx1, recty1,
                                                      rectx2 - rectx1, recty2 - recty1));
            }
        }

        cv::groupRectangles(rectList[class_num], 3, 0.2);
    }
}

void
TRT_Context::ParseResnet10Bbox(vector<cv::Rect>* rectList, int batch_th)
{
    int grid_x_ = outputDims.d[2];
    int grid_y_ = outputDims.d[1];
    int gridsize_ = grid_x_ * grid_y_;

    int target_shape[2] = {grid_x_, grid_y_};
    float bbox_norm[2] = {35.0, 35.0};
    float gc_centers_0[target_shape[0]];
    float gc_centers_1[target_shape[1]];
    for (int i = 0; i < target_shape[0]; i++)
        gc_centers_0[i] = (float)(i * 16 + 0.5)/bbox_norm[0];
    for (int i = 0; i < target_shape[1]; i++)
        gc_centers_1[i] = (float)(i * 16 + 0.5)/bbox_norm[1];
    for (int class_num = 0;
             class_num  < (g_pModelNetAttr->ParseFunc_ID == 1 ? getModelClassCnt() - 1 : getModelClassCnt());
             class_num++)
    {
        float *output_x1 = output_bbox_buf + class_num * 4 * outputDimsBBOX.d[1] * outputDimsBBOX.d[2];
        float *output_y1 = output_x1 + outputDimsBBOX.d[1] * outputDimsBBOX.d[2];
        float *output_x2 = output_y1 + outputDimsBBOX.d[1] * outputDimsBBOX.d[2];
        float *output_y2 = output_x2 + outputDimsBBOX.d[1] * outputDimsBBOX.d[2];

        for (int h = 0; h < grid_y_; h++)
        {
            for (int w = 0; w < grid_x_; w++)
            {
                int i = w + h * grid_x_;
                if (output_cov_buf[class_num * gridsize_ + i] >=
                        g_pModelNetAttr->THRESHOLD[class_num])
                {

                    float rectx1_f, recty1_f, rectx2_f, recty2_f;
                    int rectx1, recty1, rectx2, recty2;

                    rectx1_f = recty1_f = rectx2_f = recty2_f = 0.0;

                    rectx1_f = output_x1[w + h * grid_x_] - gc_centers_0[w];
                    recty1_f = output_y1[w + h * grid_x_] - gc_centers_1[h];
                    rectx2_f = output_x2[w + h * grid_x_] + gc_centers_0[w];
                    recty2_f = output_y2[w + h * grid_x_] + gc_centers_1[h];

                    rectx1_f *= (float)(-bbox_norm[0]);
                    recty1_f *= (float)(-bbox_norm[1]);
                    rectx2_f *= (float)(bbox_norm[0]);
                    recty2_f *= (float)(bbox_norm[1]);

                    rectx1 = (int)rectx1_f;
                    recty1 = (int)recty1_f;
                    rectx2 = (int)rectx2_f;
                    recty2 = (int)recty2_f;

                    rectx1 = rectx1 < 0 ? 0 : (rectx1 >= net_width ? (net_width - 1) : rectx1);
                    rectx2 = rectx2 < 0 ? 0 : (rectx2 >= net_width ? (net_width - 1) : rectx2);
                    recty1 = recty1 < 0 ? 0 : (recty1 >= net_height ? (net_height - 1) : recty1);
                    recty2 = recty2 < 0 ? 0 : (recty2 >= net_height ? (net_height - 1) : recty2);

                    rectList[class_num].push_back(cv::Rect(rectx1, recty1,
                                rectx2 - rectx1, recty2 - recty1));
                }
            }
        }
        cv::groupRectangles(rectList[class_num], 1, 0.1);
    }
}

int
TRT_Context::parseNet(const string& deployfile)
{
    ifstream readfile;
    string line;
    readfile.open(deployfile, ios::in);
    if (!readfile)
    {
        return 0;
    }
    int iterator = 0;

    while (1)
    {
        getline(readfile, line);
        string::size_type index;

        index = line.find("input_dim");
        if (index ==std::string::npos)
        {
            continue;
        }

        index = line.find_first_of(":", 0);
        string first = line.substr(0, index);
        string second = line.substr(index + 1);
        switch(iterator)
        {
        case 0:  //for batch size
            // Deprecate this interface, and let APP set batch size
            //batch_size = atoi(stringtrim(second).c_str());
            //assert(batch_size > 0);
            break;

        case 1:  // for channel num in net
            channel = atoi(stringtrim(second).c_str());
            assert(channel > 0);
            break;

        case 2:  // for net's height
            net_height = atoi(stringtrim(second).c_str());
            assert(net_height > 0);
            break;
        case 3:  // for net's width
            net_width = atoi(stringtrim(second).c_str());
            assert(net_width > 0);
            break;

        default:
            break;
        }

        if (iterator == 3)
        {
            break;
        }

        iterator++;
    }

    cout<<"Net has batch_size, channel, net_height, net_width:" <<
        batch_size << " " << channel << " " << net_height << " " <<
        net_width << endl;

    readfile.close();

    return 1;
}
