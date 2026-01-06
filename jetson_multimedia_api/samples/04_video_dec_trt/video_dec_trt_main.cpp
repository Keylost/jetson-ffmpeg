/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION. All rights reserved.
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
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include <errno.h>
#include <fstream>
#include <iostream>
#include <linux/videodev2.h>
#include <malloc.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <cuda_runtime_api.h>

#include "NvUtils.h"
#include "NvCudaProc.h"
#include "nvbuf_utils.h"
#include "video_dec_trt.h"
#include "trt_inference.h"

#define USE_CPU_FOR_INTFLOAT_CONVERSION 0

#define LOAD_IMAGE_FOR_CUDA_INPUT_DEBUG 0

#define TEST_ERROR(cond, str, label) if(cond) { \
                           cerr << str << endl; \
                           error = 1; \
                           goto label; }

#define CHUNK_SIZE 4000000

#define IS_NAL_UNIT_START(buffer_ptr) \
            (!buffer_ptr[0] && !buffer_ptr[1] && \
             !buffer_ptr[2] && (buffer_ptr[3] == 1))

const char *GOOGLE_NET_DEPLOY_NAME =
             "../../data/Model/GoogleNet_one_class/GoogleNet_modified_oneClass_halfHD.prototxt";
const char *GOOGLE_NET_MODEL_NAME =
             "../../data/Model/GoogleNet_one_class/GoogleNet_modified_oneClass_halfHD.caffemodel";

#define    TRT_MODEL        RESNET_THREE_CLASS
#define    RESNET_CAR_CLASS_ID       0

using namespace std;
using namespace nvinfer1;
using namespace nvcaffeparser1;

static void
abort(AppDecContext *ctx)
{
    ctx->got_error = true;
    ctx->dec->abort();
}

static int
read_decoder_input_chunk(ifstream * stream, NvBuffer * buffer)
{
    //length is the size of the buffer in bytes
    streamsize bytes_to_read = MIN(CHUNK_SIZE, buffer->planes[0].length);

    stream->read((char *) buffer->planes[0].data, bytes_to_read);

    // It is necessary to set bytesused properly, so that decoder knows how
    // many bytes in the buffer are valid
    buffer->planes[0].bytesused = stream->gcount();
    return 0;
}


static bool
alloc_dma_buf(int* dma_fd, int width, int height,
    NvBufferLayout layout, NvBufferColorFormat colorFormat)
{
    int ret = 0;
    NvBufferCreateParams input_params = {0};
    input_params.payloadType = NvBufferPayload_SurfArray;
    input_params.width = width;
    input_params.height = height;
    input_params.layout = NvBufferLayout_Pitch;
    input_params.colorFormat = colorFormat;
    input_params.nvbuf_tag = NvBufferTag_VIDEO_DEC;

    ret = NvBufferCreateEx (dma_fd, &input_params);
    return ret == -1 ? false : true;
}

static bool
free_dma_buf(int* dma_fd)
{
    if(*dma_fd != -1)
    {
        NvBufferDestroy(*dma_fd );
        *dma_fd  = -1;
        return true;
    }
    return false;
}

static void
resChange(AppDecContext * ctx)
{
    NvVideoDecoder *dec = ctx->dec;
    struct v4l2_format format;
    struct v4l2_crop crop;
    int32_t min_dec_capture_buffers;
    int ret = 0;
    int error = 0;
    cudaError_t cu_error;
    CUresult status;
    // Get capture plane format from the decoder. This may change after
    // an resolution change event
    ret = dec->capture_plane.getFormat(format);
    TEST_ERROR(ret < 0,
               "Error: Could not get format from decoder capture plane", error);

    // Get the display resolution from the decoder
    ret = dec->capture_plane.getCrop(crop);
    TEST_ERROR(ret < 0,
               "Error: Could not get crop from decoder capture plane", error);
    ctx->dec_width = crop.c.width;
    ctx->dec_height = crop.c.height;

    cout << "Video Resolution: " << ctx->dec_width << "x" << ctx->dec_height << endl;


    // deinitPlane unmaps the buffers and calls REQBUFS with count 0
    dec->capture_plane.deinitPlane();

    // Not necessary to call VIDIOC_S_FMT on decoder capture plane.
    // But decoder setCapturePlaneFormat function updates the class variables
    ret = dec->setCapturePlaneFormat(format.fmt.pix_mp.pixelformat,
                                     format.fmt.pix_mp.width,
                                     format.fmt.pix_mp.height);
    TEST_ERROR(ret < 0, "Error in setting decoder capture plane format", error);

    // Get the minimum buffers which have to be requested on the capture plane
    ret = dec->getMinimumCapturePlaneBuffers(min_dec_capture_buffers);
    TEST_ERROR(ret < 0,
               "Error while getting value of minimum capture plane buffers",
               error);
    ctx->dma_buf_num = min_dec_capture_buffers;

    // Request min buffers, export and map buffers
    ret = dec->capture_plane.setupPlane(V4L2_MEMORY_MMAP,
                                       min_dec_capture_buffers,
                                       false,
                                       false);
    TEST_ERROR(ret < 0, "Error in decoder capture plane setup", error);
    ctx->dst_dma_fd = new int[min_dec_capture_buffers];
    ctx->eglFramePtr = new CUeglFrame[min_dec_capture_buffers];
    ctx->pResource = new CUgraphicsResource[min_dec_capture_buffers];
    ctx->egl_imagePtr = new EGLImageKHR[min_dec_capture_buffers];
    for (int i = 0; i < min_dec_capture_buffers; i++)
    {
        ret = alloc_dma_buf(ctx->dst_dma_fd + i, ctx->network_width,
                ctx->network_height,
                NvBufferLayout_Pitch,
                NvBufferColorFormat_ABGR32);
        TEST_ERROR(ret < 0, "Failed to allocate buffer", error);
        ctx->dec_output_empty_queue->push(*(ctx->dst_dma_fd + i));
        ctx->egl_imagePtr[i] = NvEGLImageFromFd(ctx->display_context->egl_display, *(ctx->dst_dma_fd + i));
        if (ctx->egl_imagePtr[i] == NULL)
        {
            cerr << "Error while mapping dmabuf fd ("
                << *(ctx->dst_dma_fd + i) << ") to EGLImage"
                << endl;
            return;
        }

        ctx->pResource[i] = NULL;
        cudaFree(0);
        status = cuGraphicsEGLRegisterImage(&(ctx->pResource[i]), ctx->egl_imagePtr[i],
            CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);
        if (status != CUDA_SUCCESS)
        {
            printf("cuGraphicsEGLRegisterImage failed: %d, cuda process stop\n",
                            status);
            return;
        }

        status = cuGraphicsResourceGetMappedEglFrame(&ctx->eglFramePtr[i], ctx->pResource[i], 0, 0);
        if (status != CUDA_SUCCESS)
        {
            printf("cuGraphicsSubResourceGetMappedArray failed\n");
        }

        ctx->dma_egl_map.insert(pair<int, CUeglFrame>(*(ctx->dst_dma_fd + i), ctx->eglFramePtr[i]));
    }
    ctx->pStream_conversion = new cudaStream_t;
    cu_error = cudaStreamCreateWithFlags(ctx->pStream_conversion, cudaStreamNonBlocking);
    if(cu_error != 0)
        cout<<"create streame failed"<<endl;
    // Capture plane STREAMON
    ret = dec->capture_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in decoder capture plane streamon", error);

    // Enqueue all the empty capture plane buffers
    for (uint32_t i = 0; i < dec->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        ret = dec->capture_plane.qBuffer(v4l2_buf, NULL);
        TEST_ERROR(ret < 0, "Error Qing buffer at output plane", error);
    }

    cout << "Resolution change successful" << endl;
    return;

error:
    if (error)
    {
        abort(ctx);
        cerr << "Error in " << __func__ << endl;
    }
}

static void*
decCaptureLoop(void *arg)
{
    AppDecContext *ctx = (AppDecContext *) arg;
    NvVideoDecoder *dec = ctx->dec;
    struct v4l2_event ev;
    int ret;

    cout << "Starting decoder capture loop thread" << endl;
    prctl (PR_SET_NAME, "decCap", 0, 0, 0);

    // Need to wait for the first Resolution change event, so that
    // the decoder knows the stream resolution and can allocate appropriate
    // buffers when we call REQBUFS
    do
    {
        ret = dec->dqEvent(ev, -1);
        if (ret < 0)
        {
            if (errno == EAGAIN)
            {
                cerr << "Timed out waiting for first V4L2_EVENT_RESOLUTION_CHANGE"
                     << endl;
            }
            else
            {
                cerr << "Error in dequeueing decoder event" << endl;
            }
            abort(ctx);
            break;
        }
    }
    while (ev.type != V4L2_EVENT_RESOLUTION_CHANGE);

    // Handle resolution change event
    if (!ctx->got_error)
        resChange(ctx);

    // Exit on error or EOS which is signalled in main()
    while (!(ctx->got_error || dec->isInError() || ctx->got_eos))
    {
        NvBuffer *dec_buffer;

        // Check for Resolution change again
        ret = dec->dqEvent(ev, false);
        if (ret == 0)
        {
            switch (ev.type)
            {
                case V4L2_EVENT_RESOLUTION_CHANGE:
                    resChange(ctx);
                    continue;
            }
        }

        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));
        v4l2_buf.m.planes = planes;

        // Dequeue a filled buffer
        if (dec->capture_plane.dqBuffer(v4l2_buf, &dec_buffer, NULL, 0))
        {
            if (errno == EAGAIN)
            {
                usleep(1000);
            }
            else
            {
                abort(ctx);
                cerr << "Error while calling dequeue at capture plane"
                     << endl;
            }
            continue;
        }

        /* Clip & Stitch can be done by adjusting rectangle */
        NvBufferRect src_rect, dest_rect;
        src_rect.top = 0;
        src_rect.left = 0;
        src_rect.width = ctx->dec_width;
        src_rect.height = ctx->dec_height;
        dest_rect.top = 0;
        dest_rect.left = 0;
        dest_rect.width = ctx->network_width;
        dest_rect.height = ctx->network_height;

        NvBufferTransformParams transform_params;
        /* Indicates which of the transform parameters are valid */
        memset(&transform_params,0,sizeof(transform_params));
        transform_params.transform_flag = NVBUFFER_TRANSFORM_FILTER;
        transform_params.transform_flip = NvBufferTransform_None;
        transform_params.transform_filter = NvBufferTransform_Filter_Smart;
        transform_params.src_rect = src_rect;
        transform_params.dst_rect = dest_rect;

        // Get an empty dma buffer from empty
        int dma_buf_fd;
        pthread_mutex_lock(&ctx->empty_queue_lock);
        while (ctx->dec_output_empty_queue->empty())
        {
            pthread_cond_wait(&ctx->empty_queue_cond, &ctx->empty_queue_lock);
        }
        dma_buf_fd = ctx->dec_output_empty_queue->front();
        ctx->dec_output_empty_queue->pop();
        pthread_mutex_unlock(&ctx->empty_queue_lock);

        // Convert Blocklinear to PitchLinear RGBA
        ret = NvBufferTransform(dec_buffer->planes[0].fd, dma_buf_fd, &transform_params);
        if (ret == -1)
        {
            cerr << "Transform failed" << endl;
            break;
        }
        pthread_mutex_lock(&ctx->filled_queue_lock);
        ctx->dec_output_filled_queue->push(dma_buf_fd);
        pthread_cond_broadcast(&ctx->filled_queue_cond);
        pthread_mutex_unlock(&ctx->filled_queue_lock);

        ret = dec->capture_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
            cout << "Error Qing buffer at output plane" << endl;
    }

    pthread_mutex_lock(&ctx->filled_queue_lock);
    ctx->dec_output_filled_queue->push(-1);
    pthread_cond_broadcast(&ctx->filled_queue_cond);
    pthread_mutex_unlock(&ctx->filled_queue_lock);

    cout << "Exiting decoder capture loop thread" << endl;
    return NULL;
}

#if LOAD_IMAGE_FOR_CUDA_INPUT_DEBUG
int
loadImage(char *fileName,
                int requiredWidth, int requiredHeight,
                unsigned char *rp, unsigned char *gp, unsigned char *bp,
                int *imageWidth, int *imageHeight)
{
    if (fileName == NULL ||
        requiredWidth <= 0 || requiredHeight <= 0 ||
        rp == NULL || gp == NULL || bp == NULL ||
        imageHeight == NULL || imageWidth == NULL)
        return -1;

    const char *imageName = fileName;
    cv::Mat src = cv::imread(imageName, CV_LOAD_IMAGE_COLOR);
    printf("input image %dx%d\n", src.cols, src.rows);

    *imageWidth = src.cols;
    *imageHeight = src.rows;

    cv::Size size(requiredWidth, requiredHeight);
    cv::Mat dst; //dst image
    cv::resize(src, dst, size); //resize image

    cv::Mat dst8U;
    dst.convertTo(dst8U, CV_8U);

    int nRows = dst8U.rows;
    int nCols = dst8U.cols;

    int y, x;
    uchar* p;
    p = dst8U.data;
    for(y = 0; y < nRows ; y++)
    {
        for (x = 0; x < nCols; ++x)
        {
            uchar b = p[dst8U.channels()*(nCols * y + x) + 0];
            uchar g = p[dst8U.channels()*(nCols * y + x) + 1];
            uchar r = p[dst8U.channels()*(nCols * y + x) + 2];

            bp[y * nCols + x] = b;
            gp[y * nCols + x] = g;
            rp[y * nCols + x] = r;
        }
    }
    return 0;
}

void
loadImageToCudaInput(char* file_name, TRT_Context* trtCtx, void* cuda_buf)
{
    int modelInputW = trtCtx->getNetWidth();
    int modelInputH = trtCtx->getNetHeight();
    int modelInputC = trtCtx->getChannel();

    unsigned char *r = NULL;
    unsigned char *g = NULL;
    unsigned char *b = NULL;
    unsigned char *d = NULL;
    int imageWidth, imageHeight;
    d = (unsigned char *) malloc(modelInputW * modelInputH * modelInputC);
    b = d;
    g = b + modelInputW * modelInputH;
    r = g + modelInputW * modelInputH;
    if (loadImage(file_name, modelInputW, modelInputH, r, g, b, &imageWidth, &imageHeight))
    {
        printf("load image failed\n");
        exit(-1);
    }

    //               b    g    r
    int offsets[] = {124, 117, 104};

    float *input = NULL;
    input = (float *)malloc(modelInputW * modelInputH * modelInputC * sizeof(float));
    int x, y, c;
    for (c = 0; c < modelInputC; c++)
    {
        for (y = 0; y < modelInputH; y++)
        {
            for (x = 0; x < modelInputW; x++)
            {
                input[c * modelInputW * modelInputH + y * modelInputW + x] =
                    (float)d[c * modelInputW * modelInputH + y * modelInputW + x] - offsets[c];
            }
        }
    }

    int status = 0;
    status =
        cudaMemcpy(cuda_buf, input,
                   modelInputW * modelInputH * modelInputC * sizeof(float),
                   cudaMemcpyHostToDevice);
    assert(status == 0);
    free(d);
    free(input);
}
#endif


static bool
extractdmabuf(AppTRTContext *ctx)
{
    AppDecContext *dec_ctx;
    void *cuda_buf = ctx->trt_ctx->getBuffer(0);
    int batch_offset;
    int dma_buf_fd;

    for (int i = 0; i < ctx->dec_num; i ++)
    {
        if(ctx->bLastframe[i] == 1)
            continue;

        dec_ctx = ctx->dec_context[i];
        pthread_mutex_lock(&dec_ctx->filled_queue_lock);
        while(dec_ctx->dec_output_filled_queue->empty())
        {
            pthread_cond_wait(&dec_ctx->filled_queue_cond, &dec_ctx->filled_queue_lock);
        }
        dma_buf_fd = dec_ctx->dec_output_filled_queue->front();
        dec_ctx->dec_output_filled_queue->pop();
        pthread_mutex_unlock(&dec_ctx->filled_queue_lock);

        if( dma_buf_fd == -1)
        {
            ctx->bLastframe[i] = 1;
            continue;
        }

        batch_offset = i * ctx->trt_ctx->getNetWidth() *
                        ctx->trt_ctx->getNetHeight() * ctx->trt_ctx->getChannel();

        // map eglimage into GPU address
        CUeglFrame eglFrame = dec_ctx->dma_egl_map.find(dma_buf_fd)->second;
        convertEglFrameIntToFloat(&eglFrame,
                    ctx->trt_ctx->getNetWidth(),
                    ctx->trt_ctx->getNetHeight(),
                    (TRT_MODEL == GOOGLENET_THREE_CLASS || TRT_MODEL == RESNET_THREE_CLASS) ? COLOR_FORMAT_BGR : COLOR_FORMAT_RGB,
                    (char *)cuda_buf + batch_offset * sizeof(float),
                    ctx->trt_ctx->getOffsets(),
                    ctx->trt_ctx->getScales(),
                    dec_ctx->pStream_conversion);

        pthread_mutex_lock(&dec_ctx->empty_queue_lock);
        dec_ctx->dec_output_empty_queue->push(dma_buf_fd);
        pthread_cond_broadcast(&dec_ctx->empty_queue_cond);
        pthread_mutex_unlock(&dec_ctx->empty_queue_lock);
    }

    for (int i = 0; i < ctx->dec_num; i ++)
    {
        cudaStreamSynchronize(*(dec_ctx->pStream_conversion));
    }
    return true;
}

static bool
eos(AppTRTContext *ctx)
{
    for (int i = 0; i < ctx->dec_num; i ++)
    {
        if(ctx->bLastframe[i] != 1)
            return false;
    }

    return true;
}

static bool
init_display(AppDisplayContext* disp_ctx)
{
    // Get default EGL display
    disp_ctx->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (disp_ctx->egl_display == EGL_NO_DISPLAY)
    {
        cerr << "Error while get EGL display connection" << endl;
        return false;
    }

    // Init EGL display connection
    if (!eglInitialize(disp_ctx->egl_display, NULL, NULL))
    {
        cerr << "Erro while initialize EGL display connection" << endl;
        return false;
    }

    return true;
}

static bool
terminate_display(AppDisplayContext* disp_ctx)
{
    // Terminate EGL display connection
    if (disp_ctx->egl_display)
    {
        if (!eglTerminate(disp_ctx->egl_display))
        {
            cerr << "Error while terminate EGL display connection\n" << endl;
            return false;
        }
    }
    return true;
}

static void*
trtThread(void *arg)
{
    AppTRTContext *ctx = (AppTRTContext *)arg;
    prctl (PR_SET_NAME, "trtThread", 0, 0, 0);
    struct timeval input_time;
    struct timeval output_time;

    static int trt_buf_num[MAX_CHANNEL];
    memset(trt_buf_num, 0, sizeof(int) * MAX_CHANNEL);

    for(int batch_th = 0; batch_th < ctx->dec_num; batch_th++)
    {
        AppDecContext* dec_ctx = ctx->dec_context[batch_th];
        sprintf(dec_ctx->output_path, "result%d.txt", batch_th);
        dec_ctx->fstream.open(dec_ctx->output_path, ios::out);
    }
    long iInferDuration = 0;
    long iWaitDuration = 0;
    static int frameNUM = 0;

    while (1)
    {
        if(extractdmabuf(ctx) == false)
        {
            cerr<<"Fetch buffer error"<<endl;
            break;
        }

        //check for all decode's frame is empty
        if(eos(ctx))
        {
            break;
        }

        frameNUM++;
        // buffer comes, begin to inference
        int classCnt = ctx->trt_ctx->getModelClassCnt();
        queue<vector<cv::Rect>> rectList_queue[classCnt];
        gettimeofday(&input_time, NULL);
        if(frameNUM != 1)
             iWaitDuration += (input_time.tv_sec - output_time.tv_sec) * 1000 +
                        (input_time.tv_usec - output_time.tv_usec) / 1000;
        ctx->trt_ctx->doInference(
            rectList_queue);
        gettimeofday(&output_time, NULL);

       iInferDuration += (output_time.tv_sec - input_time.tv_sec) * 1000 +
                        (output_time.tv_usec - input_time.tv_usec) / 1000;
        // Dump TRT inference result(car only)
        int class_num = RESNET_CAR_CLASS_ID;
        while(!rectList_queue[class_num].empty())
        {
            for(int batch_th = 0; batch_th < ctx->dec_num; batch_th++)
            {
                if (ctx->bLastframe[batch_th] == 1)
                    continue;
                vector<cv::Rect> rectList = rectList_queue[class_num].front();
                rectList_queue[class_num].pop();
                AppDecContext* dec_ctx = ctx->dec_context[batch_th];
                dec_ctx->fstream << "frame:" << trt_buf_num[batch_th]
                    << " class num:" << class_num
                    << " has rect:" << rectList.size() << endl;
                for (uint32_t i = 0; i < rectList.size(); i++)
                {
                    cv::Rect &r = rectList[i];
                    dec_ctx->fstream << "\tx,y,w,h:"
                        << (float) r.x / ctx->trt_ctx->getNetWidth() << " "
                        << (float) r.y / ctx->trt_ctx->getNetHeight() << " "
                        << (float) r.width / ctx->trt_ctx->getNetWidth() << " "
                        << (float) r.height / ctx->trt_ctx->getNetHeight() << endl;
                    if (log_level >= LOG_LEVEL_DEBUG)
                    {
                        cout << "class num " << class_num
                             <<"  x "<< r.x <<" y: " << r.y
                             <<" width "<< r.width <<" height "<< r.height
                             << endl;
                    }
                }
                dec_ctx->fstream << endl;
            }
        }
        for(int batch_th = 0; batch_th < ctx->dec_num; batch_th++)
        {
            if (ctx->bLastframe[batch_th] == 1)
               continue;
            trt_buf_num[batch_th]++;
        }
        gettimeofday(&output_time, NULL);
    }
    cout<<"Inference Performance(ms per batch):"<<iInferDuration / frameNUM <<" Wait from decode takes(ms per batch):"<< iWaitDuration /(frameNUM -1)<<endl;
    for(int batch_th = 0; batch_th < ctx->dec_num; batch_th++)
    {
        AppDecContext* dec_ctx = ctx->dec_context[batch_th];
        dec_ctx->fstream.close();
    }

    return NULL;
}


static void
setDefaults(AppDecContext * dec_ctx, AppTRTContext* trt_ctx_wrap)
{
    for (int i = 0; i < trt_ctx_wrap->dec_num; i++)
    {
        dec_ctx[i].got_error = 0;
        dec_ctx[i].got_eos = 0;
        dec_ctx[i].dec_output_empty_queue = new queue < int >;
        dec_ctx[i].dec_output_filled_queue = new queue< int >;
        dec_ctx[i].disable_dpb = false;

        pthread_mutex_init(&dec_ctx[i].empty_queue_lock, NULL);
        pthread_cond_init(&dec_ctx[i].empty_queue_cond, NULL);
        pthread_mutex_init(&dec_ctx[i].filled_queue_lock, NULL);
        pthread_cond_init(&dec_ctx[i].filled_queue_cond, NULL);
    }
}

static void*
start_decode(void *arg)
{
    AppDecContext* ctx = (AppDecContext*) arg;
    int error = 0;
    bool eos = false;
    int ret = 0;
    unsigned int i = 0;
    char capture_thread[16] = "CapturePlane";
    string s = to_string(ctx->thread_id);

    ctx->in_file = new ifstream(ctx->in_file_path);
    TEST_ERROR(!ctx->in_file->is_open(), "Error opening input file", dec_cleanup);

    // Step-1: Create Decoder
    ctx->dec = NvVideoDecoder::createVideoDecoder("dec0");
    TEST_ERROR(!ctx->dec, "Could not create decoder", dec_cleanup);

    // Subscribe to Resolution change event
    ret = ctx->dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE, 0, 0);
    TEST_ERROR(ret < 0, "Could not subscribe to V4L2_EVENT_RESOLUTION_CHANGE", dec_cleanup);

    // Set format on the output plane
    ret = ctx->dec->setOutputPlaneFormat(ctx->decoder_pixfmt, CHUNK_SIZE);
    TEST_ERROR(ret < 0, "Could not set output plane format", dec_cleanup);

    // Set V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT control to false
    // so that application can send chunks of encoded data instead of forming
    // complete frames. This needs to be done before setting format on the
    // output plane.
    ret = ctx->dec->setFrameInputMode(1);
    TEST_ERROR(ret < 0, "Error in decoder setFrameInputMode", dec_cleanup);

    // V4L2_CID_MPEG_VIDEO_DISABLE_DPB should be set after output plane
    // set format
    if (ctx->disable_dpb)
    {
        ret = ctx->dec->disableDPB();
        TEST_ERROR(ret < 0, "Error in disableDPB", dec_cleanup);
    }

    // Query, Export and Map the output plane buffers so that we can read
    // encoded data into the buffers
    ret = ctx->dec->output_plane.setupPlane(V4L2_MEMORY_MMAP, 10, true, false);
    TEST_ERROR(ret < 0, "Error while setting up output plane", dec_cleanup);

    // Start decoder after TRT
    ret = ctx->dec->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in output plane stream on", dec_cleanup);
    pthread_create(&ctx->dec_capture_loop, NULL, decCaptureLoop, ctx);
    strcat(capture_thread, s.c_str());
    pthread_setname_np(ctx->dec_capture_loop,capture_thread);

    // Step-2: Input encoded data to decoder until EOF.
    // Read encoded data and enqueue all the output plane buffers.
    // Exit loop in case file read is complete.

    while (!eos && !ctx->got_error && !ctx->dec->isInError() &&
            i < ctx->dec->output_plane.getNumBuffers())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        buffer = ctx->dec->output_plane.getNthBuffer(i);
        read_decoder_input_chunk(ctx->in_file, buffer);

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;

        // It is necessary to queue an empty buffer to signal EOS to the decoder
        // i.e. set v4l2_buf.m.planes[0].bytesused = 0 and queue the buffer
        ret = ctx->dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error Qing buffer at output plane" << endl;
            abort(ctx);
            break;
        }
        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            eos = true;
            cout << "Input file read complete" << endl;
            break;
        }
        i++;
    }

    // Since all the output plane buffers have been queued, we first need to
    // dequeue a buffer from output plane before we can read new data into it
    // and queue it again.
    while (!eos && !ctx->got_error && !ctx->dec->isInError())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;

        ret = ctx->dec->output_plane.dqBuffer(v4l2_buf, &buffer, NULL, -1);
        if (ret < 0)
        {
            cerr << "Error DQing buffer at output plane" << endl;
            abort(ctx);
            break;
        }

        read_decoder_input_chunk(ctx->in_file, buffer);

        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;
        ret = ctx->dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error Qing buffer at output plane" << endl;
            abort(ctx);
            break;
        }
        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            eos = true;
            cout << "Input file read complete" << endl;
            break;
        }
    }

    // Step-3: Handling EOS.
    // After sending EOS, all the buffers from output plane should be dequeued.
    // and after that capture plane loop should be signalled to stop.
    while (ctx->dec->output_plane.getNumQueuedBuffers() > 0 &&
           !ctx->got_error && !ctx->dec->isInError())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;
        ret = ctx->dec->output_plane.dqBuffer(v4l2_buf, NULL, NULL, -1);
        if (ret < 0)
        {
            cerr << "Error DQing buffer at output plane" << endl;
            abort(ctx);
            break;
        }
    }
	//Signal EOS to the decoder capture loop
    ctx->got_eos = true;
    return NULL;

dec_cleanup:
    if (error == 1)
        cout<<"Decode error, go to clean up"<<endl;
    return NULL;
}

static int
exit_decode(AppDecContext* ctx)
{
    if (ctx->dec_capture_loop)
    {
        pthread_join(ctx->dec_capture_loop, NULL);
    }

    for(int i = 0; i < ctx->dma_buf_num; i++)
    {
        // Destroy EGLImage
        NvDestroyEGLImage(ctx->display_context->egl_display, ctx->egl_imagePtr[i]);
        ctx->egl_imagePtr[i] = NULL;

        CUresult status;
        status = cuGraphicsUnregisterResource(ctx->pResource[i]);
        if (status != CUDA_SUCCESS)
        {
            printf("cuGraphicsEGLUnRegisterResource failed: %d\n", status);
        }

        free_dma_buf(ctx->dst_dma_fd +i);
    }
    cudaStreamDestroy(*(ctx->pStream_conversion));
    delete ctx->pStream_conversion;
    delete []ctx->dst_dma_fd;
    delete []ctx->eglFramePtr;
    delete []ctx->pResource;
    delete []ctx->egl_imagePtr;
    delete ctx->dec_output_empty_queue;
    delete ctx->dec_output_filled_queue;

    if (ctx->dec && ctx->dec->isInError())
    {
        cerr << "Decoder is in error" << endl;
        return -1;
    }
    if (ctx->got_error)
    {
        return -1;
    }

    // The decoder destructor does all the cleanup i.e set streamoff on output and capture planes,
    // unmap buffers, tell decoder to deallocate buffer (reqbufs ioctl with counnt = 0),
    // and finally call v4l2_close on the fd.
    delete ctx->dec;
    delete ctx->in_file;
    free(ctx->in_file_path);

    return 1;
}


int
main(int argc, char *argv[])
{
    AppDecContext ctx[MAX_CHANNEL];
    AppTRTContext trt_ctx_wrap;
    AppDisplayContext disp_ctx;
    int ret = 0;
    int error = 0;
    int i;


    trt_ctx_wrap.trt_ctx = new TRT_Context;
    trt_ctx_wrap.trt_ctx->setModelIndex(TRT_MODEL);
    trt_ctx_wrap.trt_ctx->setDumpResult(true);

    if (parseCsvArgs(ctx, &trt_ctx_wrap, argc, argv))
    {
        cerr << "Error parsing commandline arguments." << endl;
        return -1;
    }

    setDefaults(ctx, &trt_ctx_wrap);

    // Initialize EGL DISPLAY
    ret = init_display(&disp_ctx);
    TEST_ERROR(ret < 0, "Initialize display failed", cleanup);

    // Give the decoder&Display's pointer to TRT
    for(i = 0; i < trt_ctx_wrap.dec_num; i++)
    {
        trt_ctx_wrap.dec_context[i] = ctx + i;
        trt_ctx_wrap.dec_context[i]->display_context = &disp_ctx;
    }
    trt_ctx_wrap.display_context = &disp_ctx;

    trt_ctx_wrap.trt_ctx->setBatchSize(trt_ctx_wrap.dec_num);

    // Create TRT
    if (trt_ctx_wrap.onnxmodelfile.empty())
    {
        trt_ctx_wrap.trt_ctx->buildTrtContext(trt_ctx_wrap.deployfile,
                                                trt_ctx_wrap.modelfile);
    }
    else
    {
        trt_ctx_wrap.trt_ctx->buildTrtContext(trt_ctx_wrap.deployfile, trt_ctx_wrap.onnxmodelfile, false, true);
    }
    pthread_create(&trt_ctx_wrap.trt_thread_handle, NULL, trtThread, &trt_ctx_wrap);
    pthread_setname_np(trt_ctx_wrap.trt_thread_handle,"TRTThread");

    for( i = 0; i < trt_ctx_wrap.dec_num; i++)
    {
        ctx[i].network_width = trt_ctx_wrap.trt_ctx->getNetWidth();
        ctx[i].network_height = trt_ctx_wrap.trt_ctx->getNetHeight();
        ctx[i].thread_id = i;

        pthread_create(&ctx[i].dec_output_loop, NULL,
                start_decode, ctx + i);
        char output_thread[16] = "OutputPlane";
        string s = to_string(i);
        strcat(output_thread, s.c_str());
        pthread_setname_np(ctx[i].dec_output_loop,output_thread);
    }

cleanup:
    // This should be done before decode, becasue decode&&TRT share buffer
    pthread_join(trt_ctx_wrap.trt_thread_handle, NULL);
    trt_ctx_wrap.trt_ctx->destroyTrtContext();
    delete trt_ctx_wrap.trt_ctx;

    // Destroy all decoders
    for(i = 0; i < trt_ctx_wrap.dec_num; i++)
    {
        ret = exit_decode(ctx + i);
        if( ret == -1 )
            error = 1;
    }

    // Destroy EGL DISPLAY
    ret = terminate_display(&disp_ctx);
    if (!ret)
    {
        error = 1;
        cerr << "Terminate display failed" << endl;
    }

    if (error)
    {
        cout << "App run failed" << endl;
    }
    else
    {
        cout << "App run was successful" << endl;
    }
    return -error;
}
