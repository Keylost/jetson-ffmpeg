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

#include "NvUtils.h"
#include <errno.h>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <linux/videodev2.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "NvCudaProc.h"
#include "nvbuf_utils.h"
#include "v4l2_nv_extensions.h"
#include "v4l2_backend_test.h"
#include <sys/time.h>

#ifdef ENABLE_TRT
#include "trt_inference.h"

#define    TRT_MODEL        GOOGLENET_SINGLE_CLASS

#endif

#define TEST_ERROR(cond, str, label) if(cond) { \
                                        cerr << str << endl; \
                                        error = 1; \
                                        goto label; }

#define CHUNK_SIZE 4000000
#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define NAL_UNIT_START_CODE 0x00000001
#define MIN_CHUNK_SIZE      50
#define USE_CPU_FOR_INTFLOAT_CONVERSION 0

static const int IMAGE_WIDTH = 1920;
static const int IMAGE_HEIGHT = 1080;
const char *GOOGLE_NET_DEPLOY_NAME =
        "../../data/Model/GoogleNet_one_class/GoogleNet_modified_oneClass_halfHD.prototxt";
const char *GOOGLE_NET_MODEL_NAME =
        "../../data/Model/GoogleNet_one_class/GoogleNet_modified_oneClass_halfHD.caffemodel";

#define IS_NAL_UNIT_START(buffer_ptr) (!buffer_ptr[0] && !buffer_ptr[1] && \
        !buffer_ptr[2] && (buffer_ptr[3] == 1))

#define IS_NAL_UNIT_START1(buffer_ptr) (!buffer_ptr[0] && !buffer_ptr[1] && \
        (buffer_ptr[2] == 1))

using namespace std;

#ifdef ENABLE_TRT
#define OSD_BUF_NUM 100
static int frame_num = 1;  //this is used to filter image feeding to TRT

//following struture is used to conmmunicate between TRT thread and
//V4l2 capture thread
queue<Shared_Buffer> TRT_Buffer_Queue;
pthread_mutex_t      TRT_lock; // for dec and conv
pthread_cond_t       TRT_cond;
int                  TRT_Stop = 0;
pthread_t            TRT_Thread_handle;

using namespace nvinfer1;
using namespace nvcaffeparser1;

TRT_Context g_trt_context;
void *trt_thread(void *data);

#endif

EGLDisplay egl_display;
jpeg_enc_context_t g_jpeg_enc_context;

static uint64_t ts[CHANNEL_NUM];
static uint64_t time_scale[CHANNEL_NUM];

static int
init_jpeg_context()
{
    pthread_mutex_init(&g_jpeg_enc_context.queue_lock, NULL);
    g_jpeg_enc_context.JpegEnc = NvJPEGEncoder::createJPEGEncoder("jpeg_enc");

    if (g_jpeg_enc_context.JpegEnc == NULL)
    {
        cout<<"create jpeg encode failed"<<endl;
        return 0;
    }

    g_jpeg_enc_context.pbuf = new unsigned char[JPEG_ENC_BUF_SIZE];
    g_jpeg_enc_context.buf_size = JPEG_ENC_BUF_SIZE;
    memset(g_jpeg_enc_context.filename, 0,
            sizeof(g_jpeg_enc_context.filename));

    return 1;
}

static int
destroy_jpeg_context()
{
    delete []g_jpeg_enc_context.pbuf;
    if (g_jpeg_enc_context.JpegEnc)
        delete g_jpeg_enc_context.JpegEnc;

    return 1;
}

static int
read_decoder_input_nalu(ifstream * stream, NvBuffer * buffer,
        char *parse_buffer, streamsize parse_buffer_size)
{
    // Length is the size of the buffer in bytes
    char *buffer_ptr = (char *) buffer->planes[0].data;

    char *stream_ptr;
    bool nalu_found = false;

    streamsize bytes_read;
    streamsize stream_initial_pos = stream->tellg();

    stream->read(parse_buffer, parse_buffer_size);
    bytes_read = stream->gcount();

    if (bytes_read == 0)
    {
        return buffer->planes[0].bytesused = 0;
    }

    // Find the first NAL unit in the buffer
    stream_ptr = parse_buffer;
    while ((stream_ptr - parse_buffer) < (bytes_read - 3))
    {
        nalu_found = IS_NAL_UNIT_START(stream_ptr) ||
                        IS_NAL_UNIT_START1(stream_ptr);
        if (nalu_found)
        {
            break;
        }
        stream_ptr++;
    }

    // Reached end of buffer but could not find NAL unit
    if (!nalu_found)
    {
        cerr << "Could not read nal unit from file. EOF or file corrupted"
            << endl;
        return -1;
    }

    memcpy(buffer_ptr, stream_ptr, 4);
    buffer_ptr += 4;
    buffer->planes[0].bytesused = 4;
    stream_ptr += 4;

    // Copy bytes till the next NAL unit is found
    while ((stream_ptr - parse_buffer) < (bytes_read - 3))
    {
        if (IS_NAL_UNIT_START(stream_ptr) || IS_NAL_UNIT_START1(stream_ptr))
        {
            streamsize seekto = stream_initial_pos +
                    (stream_ptr - parse_buffer);
            if (stream->eof())
            {
                stream->clear();
            }
            stream->seekg(seekto, stream->beg);
            return 0;
        }
        *buffer_ptr = *stream_ptr;
        buffer_ptr++;
        stream_ptr++;
        buffer->planes[0].bytesused++;
    }

    // Reached end of buffer but could not find NAL unit
    cerr << "Could not read nal unit from file. EOF or file corrupted"
            << endl;
    return -1;
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

static int
init_decode_ts()
{
    for (uint32_t i = 0; i < CHANNEL_NUM; i++)
    {
        ts[i] = 0L;
        time_scale[i] = 33000 * 10;
    }

    return 1;
}

static int
assign_decode_ts(struct v4l2_buffer *v4l2_buf, uint32_t channel)
{
    v4l2_buf->timestamp.tv_sec = ts[channel] + time_scale[channel];
    ts[channel] += time_scale[channel];

    return 1;
}

static nal_type_e
parse_nalu_unit(NvBuffer * buffer)
{
    unsigned char *pbuf = buffer->planes[0].data;

    return (nal_type_e)(*(pbuf + 4) & 0x1F);
}

static int
wait_for_nextFrame(context_t * ctx)
{
    if (ctx->cpu_occupation_option == PARSER_DECODER_VIC_RENDER)
        return 1;

    pthread_mutex_lock(&ctx->fps_lock);
    uint64_t decode_time_usec;
    uint64_t decode_time_sec;
    uint64_t decode_time_nsec;
    struct timespec last_decode_time;
    struct timeval now;
    gettimeofday(&now, NULL);

    last_decode_time.tv_sec = now.tv_sec;
    last_decode_time.tv_nsec = now.tv_usec * 1000L;

    decode_time_usec = 1000000L / ctx->fps;
    decode_time_sec = decode_time_usec / 1000000;
    decode_time_nsec = (decode_time_usec % 1000000) * 1000L;

    last_decode_time.tv_sec += decode_time_sec;
    last_decode_time.tv_nsec += decode_time_nsec;
    last_decode_time.tv_sec += last_decode_time.tv_nsec / 1000000000UL;
    last_decode_time.tv_nsec %= 1000000000UL;

    pthread_cond_timedwait(&ctx->fps_cond, &ctx->fps_lock,
                &last_decode_time);
    pthread_mutex_unlock(&ctx->fps_lock);

    return 1;
}

static void
abort(context_t *ctx)
{
    ctx->got_error = true;
    ctx->dec->abort();
    if (ctx->conv)
    {
        ctx->conv->abort();
        pthread_cond_broadcast(&ctx->queue_cond);
    }
#ifdef ENABLE_TRT
    if (ctx->conv1)
    {
        ctx->conv1->abort();
        pthread_cond_broadcast(&ctx->queue1_cond);
    }
#endif
}

static bool
conv_output_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                   NvBuffer * buffer, NvBuffer * shared_buffer,
                                   void *arg)
{
    context_t *ctx = (context_t *) arg;
    struct v4l2_buffer dec_capture_ret_buffer;
    struct v4l2_plane planes[MAX_PLANES];

    if (!v4l2_buf)
    {
        cerr << "Error while dequeueing conv output plane buffer" << endl;
        abort(ctx);
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0)
    {
        return false;
    }

    memset(&dec_capture_ret_buffer, 0, sizeof(dec_capture_ret_buffer));
    memset(planes, 0, sizeof(planes));

    dec_capture_ret_buffer.index = shared_buffer->index;
    dec_capture_ret_buffer.m.planes = planes;

    pthread_mutex_lock(&ctx->queue_lock);
    ctx->conv_output_plane_buf_queue->push(buffer);

    // Return the buffer dequeued from converter output plane
    // back to decoder capture plane
    if (ctx->dec->capture_plane.qBuffer(dec_capture_ret_buffer, NULL) < 0)
    {
        pthread_cond_broadcast(&ctx->queue_cond);
        pthread_mutex_unlock(&ctx->queue_lock);
        return false;
    }

    pthread_cond_broadcast(&ctx->queue_cond);
    pthread_mutex_unlock(&ctx->queue_lock);

    return true;
}

#ifdef ENABLE_TRT
static bool
conv1_output_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                   NvBuffer * buffer, NvBuffer * shared_buffer,
                                   void *arg)
{
    context_t *ctx = (context_t *) arg;

    if (!v4l2_buf)
    {
        cerr << "Error while dequeueing conv output plane buffer" << endl;
        abort(ctx);
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0)
    {
        return false;
    }

    pthread_mutex_lock(&ctx->queue1_lock);
    ctx->conv1_output_plane_buf_queue->push(buffer);

    pthread_cond_broadcast(&ctx->queue1_cond);
    pthread_mutex_unlock(&ctx->queue1_lock);

    return true;
}
#endif

static void *render_thread(void* arg)
{
    context_t *ctx = (context_t *) arg;
    Shared_Buffer trt_buffer;
#ifdef ENABLE_TRT
    frame_bbox *bbox = NULL;
    frame_bbox temp_bbox;
    temp_bbox.g_rect_num = 0;
    temp_bbox.g_rect = new NvOSD_RectParams[OSD_BUF_NUM];
#endif
    while (1)
    {
        // waiting for buffer to come
        pthread_mutex_lock(&ctx->render_lock);
        while (ctx->render_buf_queue->empty())
        {
            pthread_cond_wait(&ctx->render_cond, &ctx->render_lock);
        }
        //pop up buffer from queue to process
        trt_buffer = ctx->render_buf_queue->front();
        ctx->render_buf_queue->pop();
        if(trt_buffer.buffer == NULL)
        {
            pthread_mutex_unlock(&ctx->render_lock);
            break;
        }
        pthread_mutex_unlock(&ctx->render_lock);

        struct v4l2_buffer *v4l2_buf = &trt_buffer.v4l2_buf;
        NvBuffer *buffer             = trt_buffer.buffer;

        if (ctx->cpu_occupation_option != PARSER_DECODER_VIC)
        {
#ifndef ENABLE_TRT
            // Create EGLImage from dmabuf fd
            ctx->egl_image = NvEGLImageFromFd(egl_display,
                                                buffer->planes[0].fd);
            if (ctx->egl_image == NULL)
            {
                cerr << "Error while mapping dmabuf fd (" <<
                        buffer->planes[0].fd << ") to EGLImage" << endl;
                return NULL;
            }

            // Running algo process with EGLImage via GPU multi cores
            HandleEGLImage(&ctx->egl_image);

            // Destroy EGLImage
            NvDestroyEGLImage(egl_display, ctx->egl_image);
            ctx->egl_image = NULL;
#else
            // Render thread is waiting for result, wait here
            if (trt_buffer.bProcess  == 1)
            {
                sem_wait(&ctx->result_ready_sem);
                pthread_mutex_lock(&ctx->osd_lock);
                if (ctx->osd_queue->size() != 0)
                {
                    bbox = ctx->osd_queue->front();
                    if (bbox != NULL)
                    {
                        temp_bbox.g_rect_num = bbox->g_rect_num;
                        memcpy(temp_bbox.g_rect, bbox->g_rect,
                            OSD_BUF_NUM * sizeof(NvOSD_RectParams));
                        delete []bbox->g_rect;
                        delete bbox;
                        bbox = NULL;
                    }
                    ctx->osd_queue->pop();
                }
                pthread_mutex_unlock(&ctx->osd_lock);
            }

            if (temp_bbox.g_rect_num != 0)
            {
                nvosd_draw_rectangles(ctx->nvosd_context, MODE_HW,
                    buffer->planes[0].fd, temp_bbox.g_rect_num, temp_bbox.g_rect);
            }
#endif
            // EglRenderer requires the fd of the 0th plane to render
            ctx->renderer->render(buffer->planes[0].fd);
            // Write raw video frame to file and return the buffer to converter
            // capture plane
            if (ctx->out_file)
                write_video_frame(ctx->out_file, *buffer);
        }

        if (ctx->conv->capture_plane.qBuffer(*v4l2_buf, NULL) < 0)
        {
            return NULL;
        }
    }
#ifdef ENABLE_TRT
    delete []temp_bbox.g_rect;
#endif
    return NULL;
}

static bool
conv_capture_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                    NvBuffer * buffer,
                                    NvBuffer * shared_buffer,
                                    void *arg)
{
    context_t *ctx = (context_t *) arg;
    Shared_Buffer batch_buffer;
    batch_buffer.bProcess = 0;

    if (!v4l2_buf)
    {
        cerr << "Error while dequeueing conv output plane buffer" << endl;
        abort(ctx);
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0)
    {
        // Use buffer = NULL to indicate EOS
        batch_buffer.buffer = NULL;
        pthread_mutex_lock(&ctx->render_lock);
        ctx->render_buf_queue->push(batch_buffer);
        pthread_cond_broadcast(&ctx->render_cond);
        pthread_mutex_unlock(&ctx->render_lock);
        return false;
    }

#ifdef ENABLE_TRT
    // here we only queue buffer for TRT process to conv1
    if (ctx->channel < g_trt_context.getNumTrtInstances() &&
            (frame_num++ % g_trt_context.getFilterNum()) == 0)
    {
        int ret;
        struct v4l2_buffer conv_capture_ret_buffer;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&conv_capture_ret_buffer, 0, sizeof(conv_capture_ret_buffer));
        memset(planes, 0, sizeof(planes));

        NvBuffer *conv1_buffer;
        pthread_mutex_lock(&ctx->queue1_lock);
        while (ctx->conv1_output_plane_buf_queue->empty())
        {
            pthread_cond_wait(&ctx->queue1_cond, &ctx->queue1_lock);
        }
        conv1_buffer = ctx->conv1_output_plane_buf_queue->front();
        ctx->conv1_output_plane_buf_queue->pop();
        pthread_mutex_unlock(&ctx->queue1_lock);
        conv_capture_ret_buffer.index = conv1_buffer->index;
        conv_capture_ret_buffer.m.planes = planes;

        ret = ctx->conv1->output_plane.qBuffer(conv_capture_ret_buffer,
                buffer);
        if (ret < 0)
        {
            ctx->got_error = true;
            return false;
        }
        batch_buffer.bProcess = 1;
    }
#endif

    // v4l2_buf is local in the DQthread and exists in the scope of the callback
    // function only and not in the entire application. The application has to
    // copy this for using at out of the callback.
    memcpy(&batch_buffer.v4l2_buf, v4l2_buf, sizeof(v4l2_buffer));

    batch_buffer.buffer = buffer;
    batch_buffer.shared_buffer = shared_buffer;
    batch_buffer.arg = arg;
    pthread_mutex_lock(&ctx->render_lock);
    ctx->render_buf_queue->push(batch_buffer);
    pthread_cond_broadcast(&ctx->render_cond);
    pthread_mutex_unlock(&ctx->render_lock);

    return true;
}


#ifdef ENABLE_TRT
void *trt_thread(void *data)
{
    uint32_t buf_num = 0;
    EGLImageKHR egl_image = NULL;
    struct v4l2_buffer *v4l2_buf;
    NvBuffer *buffer;
    context_t *ctx = NULL;
    int process_last_batch = 0;
#if USE_CPU_FOR_INTFLOAT_CONVERSION
    float *trt_inputbuf = g_trt_context.getInputBuf();
#endif
    int classCnt = g_trt_context.getModelClassCnt();

    while (1)
    {
        // wait for buffer for process to come
        Shared_Buffer trt_buffer;
        pthread_mutex_lock(&TRT_lock);
        while (TRT_Buffer_Queue.empty())
        {
            pthread_cond_wait(&TRT_cond, &TRT_lock);
        }
        if (TRT_Buffer_Queue.size() != 0)
        {
            trt_buffer = TRT_Buffer_Queue.front();
            TRT_Buffer_Queue.pop();
            if( trt_buffer.buffer == NULL)
            {
                process_last_batch = 1;
            }
        }
        pthread_mutex_unlock(&TRT_lock);
        // we still have buffer, so accumulate buffer into batch
        if (process_last_batch == 0)
        {
            v4l2_buf           = &trt_buffer.v4l2_buf;
            buffer             = trt_buffer.buffer;
            ctx                = (context_t *)trt_buffer.arg;

            int batch_offset = buf_num  * g_trt_context.getNetWidth() *
                g_trt_context.getNetHeight() * g_trt_context.getChannel();
#if USE_CPU_FOR_INTFLOAT_CONVERSION
            // copy with CPU is much slower than GPU
            // but still keep it just in case customer want to save GPU
            //generate input buffer for first time
            unsigned char *data = buffer->planes[0].data;
            int channel_offset = g_trt_context.getNetWidth() *
                            g_trt_context.getNetHeight();
            // copy buffer into input_buf
            for (int i = 0; i < g_trt_context.getChannel(); i++)
            {
                for (int j = 0; j < g_trt_context.getNetHeight(); j++)
                {
                    for (int k = 0; k < g_trt_context.getNetWidth(); k++)
                    {
                        int total_offset = batch_offset + channel_offset * i +
                            j * g_trt_context.getNetWidth() + k;
                        trt_inputbuf[total_offset] =
                            (float)(*(data + j * buffer->planes[0].fmt.stride
                            + k * 4 + 3 - i - 1));
                    }
                }
            }
#else
            // map fd into EGLImage, then copy it with GPU in parallel
            // Create EGLImage from dmabuf fd
            egl_image = NvEGLImageFromFd(egl_display,
                                            buffer->planes[0].fd);
            if (egl_image == NULL)
            {
                cerr << "Error while mapping dmabuf fd (" <<
                    buffer->planes[0].fd << ") to EGLImage" << endl;
                return NULL;
            }

            void *cuda_buf = g_trt_context.getBuffer(0);
            // map eglimage into GPU address
            mapEGLImage2Float(&egl_image,
                g_trt_context.getNetWidth(),
                g_trt_context.getNetHeight(),
                (TRT_MODEL == GOOGLENET_THREE_CLASS) ? COLOR_FORMAT_BGR : COLOR_FORMAT_RGB,
                (char *)cuda_buf + batch_offset * sizeof(float),
                g_trt_context.getOffsets(),
                g_trt_context.getScales());

            // Destroy EGLImage
            NvDestroyEGLImage(egl_display, egl_image);
            egl_image = NULL;
#endif
            buf_num++;
            // now we push it to capture plane to let v4l2 go on
            if (ctx->conv1->capture_plane.qBuffer(*v4l2_buf, NULL) < 0)
            {
                cout<<"conv1 queue buffer error"<<endl;
            }
            // buffer is not enough for a batch, continue to wait for buffer
            if (buf_num < g_trt_context.getBatchSize())
            {
                continue;
            }
        }
        // if framenum equal batch_size * n
        else if(buf_num == 0)
        {
            break;
        }

        // buffer comes, we begin to inference
        buf_num = 0;
        queue<vector<cv::Rect>> rectList_queue[classCnt];
#if USE_CPU_FOR_INTFLOAT_CONVERSION
        g_trt_context.doInference(
            rectList_queue, trt_inputbuf);
#else
        g_trt_context.doInference(
            rectList_queue);
#endif

        for (int i = 0; i < classCnt; i++)
        {
            assert(rectList_queue[i].size() == g_trt_context.getBatchSize());
        }

        while (!rectList_queue[0].empty())
        {
            int rectNum = 0;
            frame_bbox *bbox = new frame_bbox;
            bbox->g_rect_num = 0;
            bbox->g_rect = new NvOSD_RectParams[OSD_BUF_NUM];

            for (int class_num = 0; class_num < classCnt; class_num++)
            {
                vector<cv::Rect> rectList = rectList_queue[class_num].front();
                rectList_queue[class_num].pop();
                for (uint32_t i = 0; i < rectList.size(); i++)
                {
                    cv::Rect &r = rectList[i];
                    if ((r.width * IMAGE_WIDTH / g_trt_context.getNetWidth() < 10) ||
                        (r.height * IMAGE_HEIGHT / g_trt_context.getNetHeight() < 10))
                        continue;
                    bbox->g_rect[rectNum].left =
                        (unsigned int) (r.x * IMAGE_WIDTH / g_trt_context.getNetWidth());
                    bbox->g_rect[rectNum].top =
                        (unsigned int) (r.y * IMAGE_HEIGHT / g_trt_context.getNetHeight());
                    bbox->g_rect[rectNum].width =
                        (unsigned int) (r.width * IMAGE_WIDTH / g_trt_context.getNetWidth());
                    bbox->g_rect[rectNum].height =
                        (unsigned int) (r.height * IMAGE_HEIGHT / g_trt_context.getNetHeight());
                    bbox->g_rect[rectNum].border_width = 5;
                    bbox->g_rect[rectNum].has_bg_color = 0;
                    bbox->g_rect[rectNum].border_color.red = ((class_num == 0) ? 1.0f : 0.0);
                    bbox->g_rect[rectNum].border_color.green = ((class_num == 1) ? 1.0f : 0.0);
                    bbox->g_rect[rectNum].border_color.blue = ((class_num == 2) ? 1.0f : 0.0);
                    rectNum++;
                }
            }
            bbox->g_rect_num = rectNum;
            pthread_mutex_lock(&ctx->osd_lock);
            ctx->osd_queue->push(bbox);
            pthread_mutex_unlock(&ctx->osd_lock);
            //TRT has prepared result, notify here
            sem_post(&ctx->result_ready_sem);
        }

        if (process_last_batch == 1)
        {
            break;
        }
    }

    return NULL;
}

static bool
conv1_capture_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                    NvBuffer *buffer,
                                    NvBuffer *shared_buffer,
                                    void *arg)
{
    context_t *ctx = (context_t *) arg;
    //push buffer to process queue
    Shared_Buffer trt_buffer;

    if (!v4l2_buf)
    {
        cerr << "Error while dequeueing conv output plane buffer" << endl;
        abort(ctx);
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0)
    {
        // NULL indicate EOS
        trt_buffer.buffer = NULL;
        pthread_mutex_lock(&TRT_lock);
        TRT_Buffer_Queue.push(trt_buffer);
        pthread_cond_broadcast(&TRT_cond);
        pthread_mutex_unlock(&TRT_lock);
        return false;
    }

    // v4l2_buf is local in the DQthread and exists in the scope of the callback
    // function only and not in the entire application. The application has to
    // copy this for using at out of the callback.
    memcpy(&trt_buffer.v4l2_buf, v4l2_buf, sizeof(v4l2_buffer));

    trt_buffer.buffer = buffer;
    trt_buffer.shared_buffer = shared_buffer;
    trt_buffer.arg = arg;
    pthread_mutex_lock(&TRT_lock);
    TRT_Buffer_Queue.push(trt_buffer);
    pthread_cond_broadcast(&TRT_cond);
    pthread_mutex_unlock(&TRT_lock);

    return true;
}
#endif

static int
sendEOStoConverter(context_t *ctx)
{
    // Check if converter is running
    if (ctx->conv->output_plane.getStreamStatus())
    {
        NvBuffer *conv_buffer;
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(&planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;
        pthread_mutex_lock(&ctx->queue_lock);
        while (ctx->conv_output_plane_buf_queue->empty())
        {
            pthread_cond_wait(&ctx->queue_cond, &ctx->queue_lock);
        }
        conv_buffer = ctx->conv_output_plane_buf_queue->front();
        ctx->conv_output_plane_buf_queue->pop();
        pthread_mutex_unlock(&ctx->queue_lock);

        v4l2_buf.index = conv_buffer->index;

        // Queue EOS buffer on converter output plane
        return ctx->conv->output_plane.qBuffer(v4l2_buf, NULL);
    }

    return 0;
}

#ifdef ENABLE_TRT
static int
sendEOStoConverter1(context_t *ctx)
{
    // Check if converter is running
    if (ctx->conv1->output_plane.getStreamStatus())
    {
        NvBuffer *conv_buffer;
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(&planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;
        pthread_mutex_lock(&ctx->queue1_lock);
        while (ctx->conv1_output_plane_buf_queue->empty())
        {
            pthread_cond_wait(&ctx->queue1_cond, &ctx->queue1_lock);
        }
        conv_buffer = ctx->conv1_output_plane_buf_queue->front();
        ctx->conv1_output_plane_buf_queue->pop();
        pthread_mutex_unlock(&ctx->queue1_lock);

        v4l2_buf.index = conv_buffer->index;

        // Queue EOS buffer on converter output plane
        return ctx->conv1->output_plane.qBuffer(v4l2_buf, NULL);
    }

    return 0;
}
#endif


static void
query_and_set_capture(context_t * ctx)
{
    NvVideoDecoder *dec = ctx->dec;
    struct v4l2_format format;
    struct v4l2_crop crop;
    int32_t min_dec_capture_buffers;
    int ret = 0;
    int error = 0;
    uint32_t window_width;
    uint32_t window_height;
#ifndef ENABLE_TRT
    char OSDcontent[512];
#endif

    // Get capture plane format from the decoder. This may change after
    // an resolution change event
    ret = dec->capture_plane.getFormat(format);
    TEST_ERROR(ret < 0,
            "Error: Could not get format from decoder capture plane", error);

    // Get the display resolution from the decoder
    ret = dec->capture_plane.getCrop(crop);
    TEST_ERROR(ret < 0,
           "Error: Could not get crop from decoder capture plane", error);

    if (ctx->cpu_occupation_option == PARSER_DECODER_VIC_RENDER)
    {
        // Destroy the old instance of renderer as resolution might changed
        delete ctx->renderer;

        if (ctx->fullscreen)
        {
            // Required for fullscreen
            window_width = window_height = 0;
        }
        else if (ctx->window_width && ctx->window_height)
        {
            // As specified by user on commandline
            window_width = ctx->window_width;
            window_height = ctx->window_height;
        }
        else
        {
            // Resolution got from the decoder
            window_width = crop.c.width;
            window_height = crop.c.height;
        }

        // If height or width are set to zero, EglRenderer creates a fullscreen
        // window
        ctx->renderer =
            NvEglRenderer::createEglRenderer("renderer0", window_width,
                                           window_height, ctx->window_x,
                                           ctx->window_y);
        TEST_ERROR(!ctx->renderer,
                   "Error in setting up renderer. "
                   "Check if X is running or run with --disable-rendering",
                   error);

        ctx->renderer->setFPS(ctx->fps);
#ifndef ENABLE_TRT
        sprintf(OSDcontent, "Channel:%d", ctx->channel);
        ctx->renderer->setOverlayText(OSDcontent, 800, 50);
#endif
    }

    // deinitPlane unmaps the buffers and calls REQBUFS with count 0
    dec->capture_plane.deinitPlane();

    // Not necessary to call VIDIOC_S_FMT on decoder capture plane.
    // But decoder setCapturePlaneFormat function updates the class variables
    ret = dec->setCapturePlaneFormat(format.fmt.pix_mp.pixelformat,
                                     format.fmt.pix_mp.width,
                                     format.fmt.pix_mp.height);
    TEST_ERROR(ret < 0, "Error in setting decoder capture plane format",
                error);

    // Get the minimum buffers which have to be requested on the capture plane
    ret =
        dec->getControl(V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
                        min_dec_capture_buffers);
    TEST_ERROR(ret < 0,
               "Error while getting value for V4L2_CID_MIN_BUFFERS_FOR_CAPTURE",
               error);

    // Request (min + 5) buffers, export and map buffers
    ret =
        dec->capture_plane.setupPlane(V4L2_MEMORY_MMAP,
                                       min_dec_capture_buffers + 5, false,
                                       false);
    TEST_ERROR(ret < 0, "Error in decoder capture plane setup", error);

    // For file write, first deinitialize output and capture planes
    // of video converter and then use the new resolution from
    // decoder event resolution change
    ret = sendEOStoConverter(ctx);
    TEST_ERROR(ret < 0,
            "Error while queueing EOS buffer on converter output",
            error);

    ctx->conv->capture_plane.waitForDQThread(2000);

    ctx->conv->output_plane.deinitPlane();
    ctx->conv->capture_plane.deinitPlane();

#ifdef ENABLE_TRT
    if (ctx->channel < g_trt_context.getNumTrtInstances())
    {
        ctx->conv1->capture_plane.waitForDQThread(2000);

        ctx->conv1->output_plane.deinitPlane();
        ctx->conv1->capture_plane.deinitPlane();
    }
#endif
    while(!ctx->conv_output_plane_buf_queue->empty())
    {
        ctx->conv_output_plane_buf_queue->pop();
    }
#ifdef ENABLE_TRT
    if (ctx->channel < g_trt_context.getNumTrtInstances())
    {
        while(!ctx->conv1_output_plane_buf_queue->empty())
        {
            ctx->conv1_output_plane_buf_queue->pop();
        }
    }
#endif
    ret = ctx->conv->setOutputPlaneFormat(format.fmt.pix_mp.pixelformat,
                                            format.fmt.pix_mp.width,
                                            format.fmt.pix_mp.height,
                                            V4L2_NV_BUFFER_LAYOUT_BLOCKLINEAR);
    TEST_ERROR(ret < 0, "Error in converter output plane set format",
                error);

    ret = ctx->conv->setCapturePlaneFormat(format.fmt.pix_mp.pixelformat,
                                            crop.c.width,
                                            crop.c.height,
                                            V4L2_NV_BUFFER_LAYOUT_PITCH);
    TEST_ERROR(ret < 0, "Error in converter capture plane set format",
                error);

    ret = ctx->conv->setCropRect(0, 0, crop.c.width, crop.c.height);
    TEST_ERROR(ret < 0, "Error while setting crop rect", error);

#ifdef ENABLE_TRT
    if (ctx->channel < g_trt_context.getNumTrtInstances())
    {
        ret = ctx->conv1->setOutputPlaneFormat(format.fmt.pix_mp.pixelformat,
                                            crop.c.width,
                                            crop.c.height,
                                            V4L2_NV_BUFFER_LAYOUT_PITCH);
        TEST_ERROR(ret < 0, "Error in converter output plane set format",
                error);

        ret = ctx->conv1->setCapturePlaneFormat(V4L2_PIX_FMT_ABGR32,
                                            g_trt_context.getNetWidth(),
                                            g_trt_context.getNetHeight(),
                                            V4L2_NV_BUFFER_LAYOUT_PITCH);
        TEST_ERROR(ret < 0, "Error in converter capture plane set format",
                error);
    }
#endif

    ret =
        ctx->conv->output_plane.setupPlane(V4L2_MEMORY_DMABUF,
                                            dec->capture_plane.
                                            getNumBuffers(), false, false);
    TEST_ERROR(ret < 0, "Error in converter output plane setup", error);

    ret =
        ctx->conv->capture_plane.setupPlane(V4L2_MEMORY_MMAP,
                                                 dec->capture_plane.
                                                 getNumBuffers(), true, false);
    TEST_ERROR(ret < 0, "Error in converter capture plane setup", error);
#ifdef ENABLE_TRT
    if (ctx->channel < g_trt_context.getNumTrtInstances())
    {
        ret =
            ctx->conv1->output_plane.setupPlane(V4L2_MEMORY_DMABUF,
                                            dec->capture_plane.
                                            getNumBuffers(), false, false);
        TEST_ERROR(ret < 0, "Error in converter output plane setup", error);

        ret =
            ctx->conv1->capture_plane.setupPlane(V4L2_MEMORY_MMAP,
                                                 dec->capture_plane.
                                                 getNumBuffers(), true, false);
        TEST_ERROR(ret < 0, "Error in converter capture plane setup", error);
    }
#endif
    ret = ctx->conv->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in converter output plane streamon", error);

    ret = ctx->conv->capture_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in converter output plane streamoff",
                error);
#ifdef ENABLE_TRT
    if (ctx->channel < g_trt_context.getNumTrtInstances())
    {
        ret = ctx->conv1->output_plane.setStreamStatus(true);
        TEST_ERROR(ret < 0, "Error in converter output plane streamon", error);

        ret = ctx->conv1->capture_plane.setStreamStatus(true);
        TEST_ERROR(ret < 0, "Error in converter output plane streamoff",
                error);
    }
#endif
    // Add all empty conv output plane buffers to conv_output_plane_buf_queue
    for (uint32_t i = 0; i < ctx->conv->output_plane.getNumBuffers(); i++)
    {
        ctx->conv_output_plane_buf_queue->push(ctx->conv->output_plane.
                getNthBuffer(i));
    }

#ifdef ENABLE_TRT
    if (ctx->channel < g_trt_context.getNumTrtInstances())
    {
        // Add all empty conv1 output plane buffers to conv1_output_plane_buf_queue
        for (uint32_t i = 0; i < ctx->conv1->output_plane.getNumBuffers(); i++)
        {
            ctx->conv1_output_plane_buf_queue->push(ctx->conv1->output_plane.
                    getNthBuffer(i));
        }
    }
#endif
    for (uint32_t i = 0; i < ctx->conv->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        ret = ctx->conv->capture_plane.qBuffer(v4l2_buf, NULL);
        TEST_ERROR(ret < 0, "Error Qing buffer at converter output plane",
                    error);
    }

#ifdef ENABLE_TRT
    if (ctx->channel < g_trt_context.getNumTrtInstances())
    {
        for (uint32_t i = 0; i < ctx->conv1->capture_plane.getNumBuffers();
            i++)
        {
            struct v4l2_buffer v4l2_buf;
            struct v4l2_plane planes[MAX_PLANES];

            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            memset(planes, 0, sizeof(planes));

            v4l2_buf.index = i;
            v4l2_buf.m.planes = planes;
            ret = ctx->conv1->capture_plane.qBuffer(v4l2_buf, NULL);
            TEST_ERROR(ret < 0, "Error Qing buffer at converter output plane",
                    error);
        }
    }
#endif
    ctx->conv->output_plane.startDQThread(ctx);
    ctx->conv->capture_plane.startDQThread(ctx);
#ifdef ENABLE_TRT
    if (ctx->channel < g_trt_context.getNumTrtInstances())
    {
        ctx->conv1->output_plane.startDQThread(ctx);
        ctx->conv1->capture_plane.startDQThread(ctx);
    }
#endif
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
    cout << "Query and set capture  successful" << endl;
    return;

error:
    if (error)
    {
        ctx->got_error = true;
        cerr << "Error in " << __func__ << endl;
    }
}

static void *
dec_capture_loop_fcn(void *arg)
{
    context_t *ctx = (context_t *) arg;
    NvVideoDecoder *dec = ctx->dec;
    map<uint64_t, frame_info_t*>::iterator  iter;
    struct v4l2_event ev;
    int ret;

    cout << "Starting decoder capture loop thread" << endl;
    // Need to wait for the first Resolution change event, so that
    // the decoder knows the stream resolution and can allocate appropriate
    // buffers when we call REQBUFS
    do
    {
        ret = dec->dqEvent(ev, 1000);
        if (ret < 0)
        {
            if (errno == EAGAIN)
            {
                cerr <<
                    "Timed out waiting for first V4L2_EVENT_RESOLUTION_CHANGE"
                    << endl;
            }
            else
            {
                cerr << "Error in dequeueing decoder event" << endl;
            }
            ctx->got_error = true;
            break;
        }
    }
    while (ev.type != V4L2_EVENT_RESOLUTION_CHANGE);

    // query_and_set_capture acts on the resolution change event
    if (!ctx->got_error)
        query_and_set_capture(ctx);

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
                    query_and_set_capture(ctx);
                    continue;
            }
        }

        while (1)
        {
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
                    usleep(5000);
                }
                else
                {
                    ctx->got_error = true;
                    cerr << "Error while calling dequeue at capture plane" <<
                        endl;
                }
                break;
            }

            if (ctx->do_stat)
            {
                iter = ctx->frame_info_map->find(v4l2_buf.timestamp.tv_sec);
                if (iter == ctx->frame_info_map->end())
                {
                    cout<<"image not return by decoder"<<endl;
                }
                else
                {
                    gettimeofday(&iter->second->output_time, NULL);
                }
            }

            if (ctx->cpu_occupation_option == PARSER_DECODER)
            {
                // Queue the buffer back once it has been used.
                if (dec->capture_plane.qBuffer(v4l2_buf, NULL) < 0)
                {
                    ctx->got_error = true;
                    cerr <<
                        "Error while queueing buffer at decoder capture plane"
                        << endl;
                    break;
                }
                continue;
            }
            // If we need to write to file, give the buffer to video converter output plane
            // instead of returning the buffer back to decoder capture plane
            NvBuffer *conv_buffer;
            struct v4l2_buffer conv_output_buffer;
            struct v4l2_plane conv_planes[MAX_PLANES];

            memset(&conv_output_buffer, 0, sizeof(conv_output_buffer));
            memset(conv_planes, 0, sizeof(conv_planes));
            conv_output_buffer.m.planes = conv_planes;

            // Get an empty conv output plane buffer from conv_output_plane_buf_queue
            pthread_mutex_lock(&ctx->queue_lock);
            while (ctx->conv_output_plane_buf_queue->empty())
            {
                pthread_cond_wait(&ctx->queue_cond, &ctx->queue_lock);
            }
            conv_buffer = ctx->conv_output_plane_buf_queue->front();
            ctx->conv_output_plane_buf_queue->pop();
            pthread_mutex_unlock(&ctx->queue_lock);

            conv_output_buffer.index = conv_buffer->index;

            if (ctx->conv->output_plane.
                qBuffer(conv_output_buffer, dec_buffer) < 0)
            {
                ctx->got_error = true;
                cerr <<
                    "Error while queueing buffer at converter output plane"
                    << endl;
                break;
            }

        }
    }

    // Send EOS to converter
    if (ctx->conv)
    {
        if (sendEOStoConverter(ctx) < 0)
        {
            cerr << "Error while queueing EOS buffer on converter output"
                 << endl;
        }
    }

    cout << "Exiting decoder capture loop thread" << endl;
    // Signal EOS to the decoder capture loop
    ctx->got_eos = true;

    //Signal VIC to wait EOS
    sem_post(&(ctx->dec_run_sem));
    return NULL;
}

static void *
dec_feed_loop_fcn(void *arg)
{
    context_t *ctx = (context_t *) arg;
    int i = 0;
    bool eos = false;
    int ret;
    char *nalu_parse_buffer = NULL;
    nal_type_e nal_type;

    if (ctx->input_nalu)
    {
        nalu_parse_buffer = new char[CHUNK_SIZE];
    }

    // Read encoded data and enqueue all the output plane buffers.
    // Exit loop in case file read is complete.
    while (!eos && !ctx->got_error && !ctx->dec->isInError() &&
        i < (int)ctx->dec->output_plane.getNumBuffers())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        buffer = ctx->dec->output_plane.getNthBuffer(i);
        if (ctx->input_nalu)
        {
            read_decoder_input_nalu(ctx->in_file, buffer,
                    nalu_parse_buffer, CHUNK_SIZE);
            wait_for_nextFrame(ctx);
            if (ctx->cpu_occupation_option == PARSER)
            {
                if (buffer->planes[0].bytesused == 0)
                {
                    cout<<"Input file read complete"<<endl;
                    //Signal VIC to wait EOS
                    sem_post(&(ctx->dec_run_sem));
                    return NULL;
                }
                else
                    continue;
            }
        }
        else
        {
            read_decoder_input_chunk(ctx->in_file, buffer);
        }

        v4l2_buf.index = i;
        if (ctx->input_nalu && ctx->do_stat)
        {
            nal_type = parse_nalu_unit(buffer);
            switch (nal_type)
            {
                case NAL_UNIT_CODED_SLICE:
                case NAL_UNIT_CODED_SLICE_DATAPART_A:
                case NAL_UNIT_CODED_SLICE_DATAPART_B:
                case NAL_UNIT_CODED_SLICE_DATAPART_C:
                case NAL_UNIT_CODED_SLICE_IDR:
                {
                    assign_decode_ts(&v4l2_buf, ctx->channel);
                    frame_info_t *frame_meta = new frame_info_t;
                    memset(frame_meta, 0, sizeof(frame_info_t));

                    frame_meta->timestamp = v4l2_buf.timestamp.tv_sec;
                    gettimeofday(&frame_meta->input_time, NULL);
                    frame_meta->nal_type = nal_type;

                    ctx->frame_info_map->insert(
                        pair< uint64_t, frame_info_t* >(
                        v4l2_buf.timestamp.tv_sec, frame_meta));
                    break;
                }
                default:
                    break;
            }
        }
        v4l2_buf.m.planes = planes;
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;
        // It is necessary to queue an empty buffer to signal EOS to the decoder
        // i.e. set v4l2_buf.m.planes[0].bytesused = 0 and queue the buffer
        ret = ctx->dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error Qing buffer at output plane" << endl;
            ctx->got_error = true;
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
            ctx->got_error = true;
            break;
        }

        if (ctx->input_nalu)
        {
            read_decoder_input_nalu(ctx->in_file, buffer,
                                    nalu_parse_buffer, CHUNK_SIZE);
            wait_for_nextFrame(ctx);
        }
        else
        {
            read_decoder_input_chunk(ctx->in_file, buffer);
        }

        if (ctx->input_nalu && ctx->do_stat)
        {
            nal_type = parse_nalu_unit(buffer);
            switch (nal_type)
            {
                case NAL_UNIT_CODED_SLICE:
                case NAL_UNIT_CODED_SLICE_DATAPART_A:
                case NAL_UNIT_CODED_SLICE_DATAPART_B:
                case NAL_UNIT_CODED_SLICE_DATAPART_C:
                case NAL_UNIT_CODED_SLICE_IDR:
                {
                    assign_decode_ts(&v4l2_buf, ctx->channel);
                    frame_info_t *frame_meta = new frame_info_t;
                    memset(frame_meta, 0, sizeof(frame_info_t));

                    frame_meta->timestamp = v4l2_buf.timestamp.tv_sec;
                    gettimeofday(&frame_meta->input_time, NULL);
                    frame_meta->nal_type = nal_type;
                    ctx->frame_info_map->insert(
                        pair< uint64_t, frame_info_t* >(
                        v4l2_buf.timestamp.tv_sec, frame_meta));
                    break;
                }
                default:
                    break;
            }
        }
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;
        ret = ctx->dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error Qing buffer at output plane" << endl;
            ctx->got_error = true;
            break;
        }
        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            eos = true;
            cout << "Input file read complete" << endl;
            break;
        }
    }

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
            ctx->got_error = true;
            break;
        }
    }

    if (ctx->input_nalu)
    {
        delete []nalu_parse_buffer;
    }

    ctx->got_eos = true;
    return NULL;
}

static void
set_defaults(context_t * ctx)
{
    memset(ctx, 0, sizeof(context_t));
    ctx->fullscreen = false;
    ctx->window_height = 0;
    ctx->window_width = 0;
    ctx->window_x = 0;
    ctx->window_y = 0;
    ctx->input_nalu = 1;
    ctx->fps = 30;
    ctx->disable_dpb = false;
    ctx->do_stat = 0;
    ctx->cpu_occupation_option = 0;
    ctx->dec_status = 0;
    ctx->conv_output_plane_buf_queue = new queue < NvBuffer * >;
#ifdef ENABLE_TRT
    if (ctx->channel < g_trt_context.getNumTrtInstances())
    {
        ctx->conv1_output_plane_buf_queue = new queue < NvBuffer * >;
    }
    pthread_mutex_init(&ctx->osd_lock, NULL);
    ctx->osd_queue = new queue <frame_bbox*>;
#endif
    ctx->render_buf_queue = new queue <Shared_Buffer>;
    ctx->stop_render = 0;
    ctx->frame_info_map = new map< uint64_t, frame_info_t* >;
    ctx->nvosd_context = NULL;
}

static void
set_globalcfg_default(global_cfg *cfg)
{
#ifdef ENABLE_TRT
    cfg->deployfile = GOOGLE_NET_DEPLOY_NAME;
    cfg->modelfile = GOOGLE_NET_MODEL_NAME;
#endif
}

static void
get_disp_resolution(display_resolution_t *res)
{
    if (NvEglRenderer::getDisplayResolution(
            res->window_width, res->window_height) < 0)
    {
        cerr << "get resolution failed, program will exit" << endl;
        exit(0);
    }

    return;
}

static void
do_statistic(context_t * ctx)
{
    uint64_t accu_latency = 0;
    uint64_t accu_frames = 0;
    struct timeval start_time;
    struct timeval end_time;
    map<uint64_t, frame_info_t*>::iterator  iter;

    memset(&start_time, 0, sizeof(start_time));
    memset(&end_time, 0, sizeof(end_time));
    for( iter = ctx->frame_info_map->begin();
            iter != ctx->frame_info_map->end(); iter++)
    {
        if (iter->second->output_time.tv_sec != 0 &&
                iter->second->input_time.tv_sec != 0)
        {
            accu_latency += (iter->second->output_time.tv_sec -
                                iter->second->input_time.tv_sec) * 1000 +
                               ( iter->second->output_time.tv_usec -
                                iter->second->input_time.tv_usec ) / 1000;
            accu_frames++;
            end_time = iter->second->output_time;
        }

        if (iter == ctx->frame_info_map->begin())
        {
            start_time = iter->second->input_time;
        }
    }

    cout << "total frames:" << accu_frames<<endl;
    cout <<"pipeline:" << ctx->channel << " avg_latency:(ms)" <<
                accu_latency / accu_frames << " fps:" <<
                accu_frames * 1000000 / (end_time.tv_sec * 1000000 +
                end_time.tv_usec - start_time.tv_sec * 1000000 -
                start_time.tv_usec ) << endl;
}

int
main(int argc, char *argv[])
{
    context_t ctx[CHANNEL_NUM];
    global_cfg cfg;
    int error = 0;
    uint32_t iterator;
    map<uint64_t, frame_info_t*>::iterator  iter;
    display_resolution_t disp_info;
    char **argp;
    set_globalcfg_default(&cfg);

    argp = argv;
    parse_global(&cfg, argc, &argp);

    if (parse_csv_args(&ctx[0],
#ifdef ENABLE_TRT
        &g_trt_context,
#endif
        argc - cfg.channel_num - 1, argp))
    {
        fprintf(stderr, "Error parsing commandline arguments\n");
        return -1;
    }

#ifdef ENABLE_TRT
    g_trt_context.setModelIndex(TRT_MODEL);
#if USE_CPU_FOR_INTFLOAT_CONVERSION
    g_trt_context.buildTrtContext(cfg.deployfile, cfg.modelfile, true);
#else
    g_trt_context.buildTrtContext(cfg.deployfile, cfg.modelfile);
#endif
    //Batchsize * FilterNum should be not bigger than buffers allocated by VIC
    if (g_trt_context.getBatchSize() * g_trt_context.getFilterNum() > 10)
    {
        fprintf(stderr,
            "Not enough buffers. Decrease trt-proc-interval and run again. Exiting\n");
#if USE_CPU_FOR_INTFLOAT_CONVERSION
        g_trt_context.destroyTrtContext(true);
#else
        g_trt_context.destroyTrtContext();
#endif
        return 0;
    }
    pthread_create(&TRT_Thread_handle, NULL, trt_thread, NULL);
    pthread_setname_np(TRT_Thread_handle,"TRTThreadHandle");
#endif

    get_disp_resolution(&disp_info);
    init_decode_ts();

    if (0)
        init_jpeg_context();

    // Get defalut EGL display
    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display == EGL_NO_DISPLAY)
    {
        cout<<"Error while get EGL display connection"<<endl;
        return -1;
    }

    // Init EGL display connection
    if (!eglInitialize(egl_display, NULL, NULL))
    {
        cout<<"Erro while initialize EGL display connection"<<endl;
        return -1;
    }

    for (iterator = 0; iterator < cfg.channel_num; iterator++)
    {
        int ret = 0;
        sem_init(&(ctx[iterator].dec_run_sem), 0, 0);
#ifdef ENABLE_TRT
        sem_init(&(ctx[iterator].result_ready_sem), 0, 0);
#endif
        set_defaults(&ctx[iterator]);

        char decname[512];
        sprintf(decname, "dec%d", iterator);
        ctx[iterator].channel = iterator;

        if (parse_csv_args(&ctx[iterator],
#ifdef ENABLE_TRT
            &g_trt_context,
#endif
            argc - cfg.channel_num - 1, argp))
        {
            fprintf(stderr, "Error parsing commandline arguments\n");
            return -1;
        }
        ctx[iterator].in_file_path = cfg.in_file_path[iterator];
        ctx[iterator].nvosd_context = nvosd_create_context();
        ctx[iterator].dec = NvVideoDecoder::createVideoDecoder(decname);
        TEST_ERROR(!ctx[iterator].dec, "Could not create decoder", cleanup);

        // Subscribe to Resolution change event
        ret = ctx[iterator].dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE,
                        0, 0);
        TEST_ERROR(ret < 0,
                "Could not subscribe to V4L2_EVENT_RESOLUTION_CHANGE",
                cleanup);

        // Set format on the output plane
        ret = ctx[iterator].dec->setOutputPlaneFormat(
                    ctx[iterator].decoder_pixfmt, CHUNK_SIZE);

        // Set V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT control to false
        // so that application can send chunks/slice of encoded data instead of
        // forming complete frames. This needs to be done before setting format
        // on the output plane.
        ret = ctx[iterator].dec->setFrameInputMode(1);
        TEST_ERROR(ret < 0, "Error in setFrameInputMode", cleanup);

        TEST_ERROR(ret < 0, "Could not set output plane format", cleanup);

        // V4L2_CID_MPEG_VIDEO_DISABLE_DPB should be set after output plane
        // set format
        if (ctx[iterator].disable_dpb)
        {
            ret = ctx[iterator].dec->disableDPB();
            TEST_ERROR(ret < 0, "Error in disableDPB", cleanup);
        }

        // Query, Export and Map the output plane buffers so that we can read
        // encoded data into the buffers
        ret = ctx[iterator].dec->output_plane.setupPlane(
                V4L2_MEMORY_MMAP, 10, true, false);
        TEST_ERROR(ret < 0, "Error while setting up output plane", cleanup);

        ctx[iterator].in_file = new ifstream(ctx[iterator].in_file_path);
        TEST_ERROR(!ctx[iterator].in_file->is_open(),
                "Error opening input file", cleanup);

        if (ctx[iterator].out_file_path)
        {
            ctx[iterator].out_file = new ofstream(ctx[iterator].out_file_path);
            TEST_ERROR(!ctx[iterator].out_file->is_open(),
                        "Error opening output file",
                        cleanup);
        }

        pthread_create(&ctx[iterator].render_feed_handle, NULL,
                                render_thread, &ctx[iterator]);
        char render_thread[16] = "RenderThread";
        string s = to_string(iterator);
        strcat(render_thread, s.c_str());
        pthread_setname_np(ctx[iterator].render_feed_handle,render_thread);
        // Create converter to convert from BL to PL for writing raw video
        // to file
        char convname[512];
        sprintf(convname, "conv%d", iterator);
        ctx[iterator].conv = NvVideoConverter::createVideoConverter(convname);
        TEST_ERROR(!ctx[iterator].conv, "Could not create video converter",
                cleanup);

        ctx[iterator].conv->output_plane.
            setDQThreadCallback(conv_output_dqbuf_thread_callback);
        ctx[iterator].conv->capture_plane.
            setDQThreadCallback(conv_capture_dqbuf_thread_callback);
#ifdef ENABLE_TRT
        if (iterator < g_trt_context.getNumTrtInstances())
        {
            sprintf(convname, "conv1-%d", iterator);
            ctx[iterator].conv1 =
                   NvVideoConverter::createVideoConverter(convname);
            TEST_ERROR(!ctx[iterator].conv1, "Could not create video converter",
                cleanup);

            ctx[iterator].conv1->output_plane.
                setDQThreadCallback(conv1_output_dqbuf_thread_callback);
            ctx[iterator].conv1->capture_plane.
                setDQThreadCallback(conv1_capture_dqbuf_thread_callback);
        }
#endif
        ret = ctx[iterator].dec->output_plane.setStreamStatus(true);
        TEST_ERROR(ret < 0, "Error in output plane stream on", cleanup);
        if (cfg.channel_num == 1)
        {
            ctx[iterator].window_width = disp_info.window_width;
            ctx[iterator].window_height = disp_info.window_height;
            ctx[iterator].window_x = 0;
            ctx[iterator].window_y = 0;
        }
        else
        {
            if (iterator == 0)
            {
                ctx[iterator].window_width = disp_info.window_width / 2;
                ctx[iterator].window_height = disp_info.window_height / 2;
                ctx[iterator].window_x = 0;
                ctx[iterator].window_y = 0;
            }
            else if (iterator == 1)
            {
                ctx[iterator].window_width = disp_info.window_width / 2;
                ctx[iterator].window_height = disp_info.window_height / 2;
                ctx[iterator].window_x = disp_info.window_width / 2;
                ctx[iterator].window_y = 0;
            }
            else if (iterator == 2)
            {
                ctx[iterator].window_width = disp_info.window_width / 2;
                ctx[iterator].window_height = disp_info.window_height / 2;
                ctx[iterator].window_x = 0;
                ctx[iterator].window_y = disp_info.window_height / 2;
            }
            else
            {
                ctx[iterator].window_width = disp_info.window_width / 2;
                ctx[iterator].window_height = disp_info.window_height / 2;
                ctx[iterator].window_x = disp_info.window_width / 2;
                ctx[iterator].window_y = disp_info.window_height / 2;
            }
        }
        if (ctx[iterator].cpu_occupation_option != PARSER)
        {
            pthread_create(&ctx[iterator].dec_capture_loop, NULL,
                                dec_capture_loop_fcn, &ctx[iterator]);
            char capture_thread[16] = "CapturePlane";
            string s2 = to_string(iterator);
            strcat(capture_thread, s2.c_str());
            pthread_setname_np(ctx[iterator].dec_capture_loop,capture_thread);
        }
        pthread_create(&ctx[iterator].dec_feed_handle, NULL,
                                dec_feed_loop_fcn, &ctx[iterator]);
        char output_thread[16] = "OutputPlane";
        string s3 = to_string(iterator);
        strcat(output_thread, s3.c_str());
        pthread_setname_np(ctx[iterator].dec_feed_handle,output_thread);
    }

cleanup:
    for (iterator = 0; iterator < cfg.channel_num; iterator++)
    {
        sem_wait(&(ctx[iterator].dec_run_sem)); //we need wait to make sure decode get EOS

        //send stop command to render, and wait it get consumed
        ctx[iterator].stop_render = 1;
        pthread_cond_broadcast(&ctx[iterator].render_cond);
        pthread_join(ctx[iterator].render_feed_handle, NULL);

#ifdef ENABLE_TRT
        if(iterator < g_trt_context.getNumTrtInstances())
        {
            int ret;
            ret = sendEOStoConverter1(&ctx[iterator]);
            if(ret < 0)
                cout<<"send EOS to conv1 failed"<<endl;
        }

        if (TRT_Stop == 0)
        {
            TRT_Stop = 1;
            pthread_cond_broadcast(&TRT_cond);
            pthread_join(TRT_Thread_handle, NULL);
        }
#endif
        ctx[iterator].conv->waitForIdle(-1);
        ctx[iterator].conv->capture_plane.stopDQThread();
        ctx[iterator].conv->output_plane.stopDQThread();
#ifdef ENABLE_TRT
        if (iterator < g_trt_context.getNumTrtInstances())
        {
            ctx[iterator].conv1->waitForIdle(-1);
            ctx[iterator].conv1->capture_plane.stopDQThread();
            ctx[iterator].conv1->output_plane.stopDQThread();
        }
#endif
        pthread_join(ctx[iterator].dec_feed_handle, NULL);
        if (ctx[iterator].cpu_occupation_option != PARSER)
            pthread_join(ctx[iterator].dec_capture_loop, NULL);

        if (ctx[iterator].dec->isInError())
        {
            cerr << "Decoder is in error" << endl;
            error = 1;
        }

        if (ctx[iterator].got_error)
        {
            error = 1;
        }

        if (ctx[iterator].do_stat)
            do_statistic(&ctx[iterator]);

        sem_destroy(&(ctx[iterator].dec_run_sem));
#ifdef ENABLE_TRT
        sem_destroy(&(ctx[iterator].result_ready_sem));
        pthread_mutex_destroy(&ctx[iterator].osd_lock);
#endif
        // The decoder destructor does all the cleanup i.e set streamoff on output and capture planes,
        // unmap buffers, tell decoder to deallocate buffer (reqbufs ioctl with counnt = 0),
        // and finally call v4l2_close on the fd.
        delete ctx[iterator].dec;
        delete ctx[iterator].conv;
        // Similarly, EglRenderer destructor does all the cleanup
        if (ctx->cpu_occupation_option == PARSER_DECODER_VIC_RENDER)
            delete ctx[iterator].renderer;
        delete ctx[iterator].in_file;
        delete ctx[iterator].out_file;
        delete ctx[iterator].conv_output_plane_buf_queue;
        delete ctx[iterator].render_buf_queue;
        if (ctx[iterator].nvosd_context)
        {
            nvosd_destroy_context(ctx[iterator].nvosd_context);
            ctx[iterator].nvosd_context = NULL;
        }
#ifdef ENABLE_TRT
        delete ctx[iterator].osd_queue;
        delete ctx[iterator].conv1;
        if (iterator < g_trt_context.getNumTrtInstances())
        {
           delete ctx[iterator].conv1_output_plane_buf_queue;
        }
#endif

        if (ctx[iterator].do_stat)
        {
            for( iter = ctx[iterator].frame_info_map->begin();
                    iter != ctx[iterator].frame_info_map->end(); iter++)
            {
                delete iter->second;
            }
        }
        delete ctx[iterator].frame_info_map;

        if (error)
        {
            cout << "App run failed" << endl;
        }
        else
        {
            cout << "App run was successful" << endl;
        }
    }
#ifdef ENABLE_TRT
#if USE_CPU_FOR_INTFLOAT_CONVERSION
    g_trt_context.destroyTrtContext(true);
#else
    g_trt_context.destroyTrtContext();
#endif
#endif
    // Terminate EGL display connection
    if (egl_display)
    {
        if (!eglTerminate(egl_display))
        {
            cout<<"Error while terminate EGL display connection";
            return -1;
        }
    }

    if (0)
        destroy_jpeg_context();

    return -error;
}
