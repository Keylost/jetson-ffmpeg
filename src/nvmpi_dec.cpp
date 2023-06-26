#include "nvmpi.h"
#include "NvVideoDecoder.h"
#include "nvUtils2NvBuf.h"
#include "NVMPI_bufPool.hpp"
#include "NVMPI_frameBuf.hpp"
#include <vector>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <queue>
#include <mutex>
#include <condition_variable>

#define CHUNK_SIZE 4000000
#define MAX_BUFFERS 32

#define TEST_ERROR(condition, message, errorCode)    \
	if (condition)                               \
{                                                    \
	std::cout<< message;			     \
}

using namespace std;

struct nvmpictx
{
	NvVideoDecoder *dec{nullptr};
	bool eos{false};
	int index{0};
	unsigned int coded_width{0};
	unsigned int coded_height{0};
	
	int numberCaptureBuffers{0};
	
	int dmaBufferFileDescriptor[MAX_BUFFERS];
	
#ifdef WITH_NVUTILS
	NvBufSurface *dmaBufferSurface[MAX_BUFFERS];
	NvBufSurfTransformConfigParams session;
#else
	NvBufferSession session;
#endif
	NvBufferTransformParams transform_params;
	NvBufferRect src_rect, dest_rect;
	
	nvPixFormat out_pixfmt;
	unsigned int decoder_pixfmt{0};
	std::thread dec_capture_loop;
	
	NVMPI_bufPool<NVMPI_frameBuf*>* framePool;
	
	//frame size params
	unsigned int frame_size[MAX_NUM_PLANES];
	unsigned int frame_linesize[MAX_NUM_PLANES];
	unsigned int frame_height[MAX_NUM_PLANES];
	
	//empty frame queue and free buffers memory
	void deinitFramePool();
	//alloc frame buffers based on frame_size data in nvmpictx
	void initFramePool();
	
	//get dst_dma buffer params and set corresponding frame size and linesize in nvmpictx
	void updateFrameSizeParams();
	void updateBufferTransformParams();
	
	void initDecoderCapturePlane(v4l2_format &format);
	/* deinitPlane unmaps the buffers and calls REQBUFS with count 0 */
	void deinitDecoderCapturePlane();
};

NvBufferColorFormat getNvColorFormatFromV4l2Format(v4l2_format &format)
{
	NvBufferColorFormat ret_cf = NvBufferColorFormat_NV12; 
	switch (format.fmt.pix_mp.colorspace)
	{
		case V4L2_COLORSPACE_SMPTE170M:
			if (format.fmt.pix_mp.quantization == V4L2_QUANTIZATION_DEFAULT)
			{
				// "Decoder colorspace ITU-R BT.601 with standard range luma (16-235)"
				ret_cf = NvBufferColorFormat_NV12;
			}
			else
			{
				//"Decoder colorspace ITU-R BT.601 with extended range luma (0-255)";
				ret_cf = NvBufferColorFormat_NV12_ER;
			}
			break;
		case V4L2_COLORSPACE_REC709:
			if (format.fmt.pix_mp.quantization == V4L2_QUANTIZATION_DEFAULT)
			{
				//"Decoder colorspace ITU-R BT.709 with standard range luma (16-235)";
				ret_cf = NvBufferColorFormat_NV12_709;
			}
			else
			{
				//"Decoder colorspace ITU-R BT.709 with extended range luma (0-255)";
				ret_cf = NvBufferColorFormat_NV12_709_ER;
			}
			break;
		case V4L2_COLORSPACE_BT2020:
			{
				//"Decoder colorspace ITU-R BT.2020";
				ret_cf = NvBufferColorFormat_NV12_2020;
			}
			break;
		default:
			if (format.fmt.pix_mp.quantization == V4L2_QUANTIZATION_DEFAULT)
			{
				//"Decoder colorspace ITU-R BT.601 with standard range luma (16-235)";
				ret_cf = NvBufferColorFormat_NV12;
			}
			else
			{
				//"Decoder colorspace ITU-R BT.601 with extended range luma (0-255)";
				ret_cf = NvBufferColorFormat_NV12_ER;
			}
			break;
	}
	return ret_cf;
}


void nvmpictx::initDecoderCapturePlane(v4l2_format &format)
{
	int ret=0;
	int32_t minimumDecoderCaptureBuffers;
	NvBufferCreateParams cParams;
	memset(&cParams, 0, sizeof(cParams));
	
	ret=dec->setCapturePlaneFormat(format.fmt.pix_mp.pixelformat,format.fmt.pix_mp.width,format.fmt.pix_mp.height);
	TEST_ERROR(ret < 0, "Error in setting decoder capture plane format", ret);

	dec->getMinimumCapturePlaneBuffers(minimumDecoderCaptureBuffers);
	TEST_ERROR(ret < 0, "Error while getting value of minimum capture plane buffers",ret);

	/* Request (min + extra) buffers, export and map buffers. */
	numberCaptureBuffers = minimumDecoderCaptureBuffers + 5;

	cParams.colorFormat = getNvColorFormatFromV4l2Format(format);
	cParams.width = coded_width;
	cParams.height = coded_height;
	cParams.layout = NvBufferLayout_BlockLinear;
#ifdef WITH_NVUTILS
	cParams.memType = NVBUF_MEM_SURFACE_ARRAY;
	cParams.memtag = NvBufSurfaceTag_VIDEO_DEC;
	
	ret = NvBufSurf::NvAllocate(&cParams, numberCaptureBuffers, dmaBufferFileDescriptor);
	TEST_ERROR(ret < 0, "Failed to create buffers", error);
	for (int index = 0; index < numberCaptureBuffers; index++)
	{
		ret = NvBufSurfaceFromFd(dmaBufferFileDescriptor[index], (void**)(&(dmaBufferSurface[index])));
		TEST_ERROR(ret < 0, "Failed to get surface for buffer", ret);
	}
#else
	cParams.payloadType = NvBufferPayload_SurfArray;
	cParams.nvbuf_tag = NvBufferTag_VIDEO_DEC;
	
	for (int index = 0; index < numberCaptureBuffers; index++)
	{
		ret = NvBufferCreateEx(&dmaBufferFileDescriptor[index], &cParams);
		TEST_ERROR(ret < 0, "Failed to create buffers", ret);
	}
#endif

    /* Request buffers on decoder capture plane. Refer ioctl VIDIOC_REQBUFS */
	dec->capture_plane.reqbufs(V4L2_MEMORY_DMABUF, numberCaptureBuffers);
	TEST_ERROR(ret < 0, "Error in decoder capture plane streamon", ret);

    /* Decoder capture plane STREAMON. Refer ioctl VIDIOC_STREAMON */
	dec->capture_plane.setStreamStatus(true);
	TEST_ERROR(ret < 0, "Error in decoder capture plane streamon", ret);

	/* Enqueue all the empty decoder capture plane buffers. */
	for (uint32_t i = 0; i < dec->capture_plane.getNumBuffers(); i++)
	{
		struct v4l2_buffer v4l2_buf;
		struct v4l2_plane planes[MAX_PLANES];

		memset(&v4l2_buf, 0, sizeof(v4l2_buf));
		memset(planes, 0, sizeof(planes));

		v4l2_buf.index = i;
		v4l2_buf.m.planes = planes;
		v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		v4l2_buf.memory = V4L2_MEMORY_DMABUF;
		v4l2_buf.m.planes[0].m.fd = dmaBufferFileDescriptor[i];

		ret = dec->capture_plane.qBuffer(v4l2_buf, NULL);
		TEST_ERROR(ret < 0, "Error Qing buffer at output plane", ret);
	}
	
	return;
}

void nvmpictx::deinitDecoderCapturePlane()
{
	int ret = 0;
	dec->capture_plane.setStreamStatus(false);
	dec->capture_plane.deinitPlane();
	for (int index = 0; index < numberCaptureBuffers; index++) //V4L2_MEMORY_DMABUF
	{
		if (dmaBufferFileDescriptor[index] != 0)
		{	
			ret = NvBufferDestroy(dmaBufferFileDescriptor[index]);
			TEST_ERROR(ret < 0, "Failed to Destroy NvBuffer", ret);
		}
	}
	return;
}

void nvmpictx::updateFrameSizeParams()
{
	//it's safe when called from respondToResolutionEvent() after initFramePool()
	NVMPI_frameBuf* fb = framePool->peekEmptyBuf();
#ifdef WITH_NVUTILS
	NvBufSurfacePlaneParams parm;
	NvBufSurfaceParams dst_dma_surface_params;
	dst_dma_surface_params = fb->dst_dma_surface->surfaceList[0];
	parm = dst_dma_surface_params.planeParams;
#else
	NvBufferParams parm;
	int ret = NvBufferGetParams(fb->dst_dma_fd, &parm);
	TEST_ERROR(ret < 0, "Failed to get dst dma buf params", ret);
#endif

	frame_linesize[0] = parm.width[0];
	frame_linesize[1] = parm.width[1];
	frame_linesize[2] =	parm.width[2];
	frame_size[0]	  = parm.psize[0];
	frame_size[1]	  =	parm.psize[1];
	frame_size[2] 	  =	parm.psize[2];
	frame_height[0]	  = parm.height[0];
	frame_height[1]	  = parm.height[1];
	frame_height[2]	  = parm.height[2];
	
	return;
}

void nvmpictx::updateBufferTransformParams()
{
	src_rect.top = 0;
	src_rect.left = 0;
	src_rect.width = coded_width;
	src_rect.height = coded_height;
	dest_rect.top = 0;
	dest_rect.left = 0;
	dest_rect.width = coded_width;
	dest_rect.height = coded_height;
	
	memset(&transform_params,0,sizeof(transform_params));
	transform_params.transform_flag = NVBUFFER_TRANSFORM_FILTER;
	transform_params.transform_flip = NvBufferTransform_None;
	transform_params.transform_filter = NvBufferTransform_Filter_Smart;
	//ctx->transform_params.transform_filter = NvBufSurfTransformInter_Nearest;
#ifdef WITH_NVUTILS
	transform_params.src_rect = &src_rect;
	transform_params.dst_rect = &dest_rect;
#else
	transform_params.src_rect = src_rect;
	transform_params.dst_rect = dest_rect;
	transform_params.session = session;
#endif
}

void nvmpictx::deinitFramePool()
{
	//TODO check that all allocated buffers returned to pool before deinit
	NVMPI_frameBuf* fb = NULL;
	
	while((fb = framePool->dqEmptyBuf()))
	{
		fb->destroy();
		delete fb;
	}
	
	while((fb = framePool->dqFilledBuf()))
	{
		fb->destroy();
		delete fb;
	}
	return;
}

void nvmpictx::initFramePool()
{
	//if(bufNumber <= 0) return false; //TODO log msg //TODO check if it's already allocated and deinit first
	NvBufferColorFormat cFmt = out_pixfmt==NV_PIX_NV12?NvBufferColorFormat_NV12: NvBufferColorFormat_YUV420;
	
	NvBufferCreateParams input_params;
	memset(&input_params, 0, sizeof(input_params));
	/* Create PitchLinear output buffer for transform. */
	input_params.width = coded_width;
	input_params.height = coded_height;
	input_params.layout = NvBufferLayout_Pitch;
	input_params.colorFormat = cFmt;
#ifdef WITH_NVUTILS
	input_params.memType = NVBUF_MEM_SURFACE_ARRAY;
	input_params.memtag = NvBufSurfaceTag_VIDEO_CONVERT;
#else
	input_params.payloadType = NvBufferPayload_SurfArray;
	input_params.nvbuf_tag = NvBufferTag_VIDEO_DEC;
#endif
	
	for(int i=0;i<MAX_BUFFERS;i++)
	{
		NVMPI_frameBuf* fb = new NVMPI_frameBuf();
		if(!fb->alloc(input_params)) break;
		framePool->qEmptyBuf(fb);
	}
	//TODO retun false on allocation failed
	return;
}

void respondToResolutionEvent(v4l2_format &format, v4l2_crop &crop,nvmpictx* ctx)
{
	int ret=0;

    /* Get capture plane format from the decoder.
       This may change after resolution change event.
       Refer ioctl VIDIOC_G_FMT */
	ret = ctx->dec->capture_plane.getFormat(format);	
	TEST_ERROR(ret < 0, "Error: Could not get format from decoder capture plane", ret);

    /* Get the display resolution from the decoder.
       Refer ioctl VIDIOC_G_CROP */
	ret = ctx->dec->capture_plane.getCrop(crop);
	TEST_ERROR(ret < 0, "Error: Could not get crop from decoder capture plane", ret);

	ctx->coded_width=crop.c.width;
	ctx->coded_height=crop.c.height;
	
	//init/reinit DecoderCapturePlane
	ctx->deinitDecoderCapturePlane();
	ctx->initDecoderCapturePlane(format);
	
	/* override default seesion. Without overriding session we wil
	   get seg. fault if decoding in forked process*/
#ifdef WITH_NVUTILS
	ctx->session.compute_mode = NvBufSurfTransformCompute_VIC;
	ctx->session.gpu_id = 0;
	ctx->session.cuda_stream = 0;
	NvBufSurfTransformSetSessionParams(&(ctx->session));
#else
	ctx->session = NvBufferSessionCreate();
#endif
	
	//alloc frame pool buffers (dst_dma buffers). TODO: check if already allocated and deinit pool first
	ctx->initFramePool();
	//get dst_dma buffer params and set corresponding frame size and linesize in nvmpictx
	ctx->updateFrameSizeParams();
	
	//reset buffer transformation params based on new resolution data
	ctx->updateBufferTransformParams();
	
	return;
}

/*
struct transFormWorker
{
	std::thread _workerThr;
	nvmpictx* _ctx = NULL;
	
	//void init(nvmpictx* ctx);
	
	void start(nvmpictx* ctx);
	void stop();
	
	bool isWaiting();
	
	void workerFnc();
};

void transFormWorker::start(nvmpictx* ctx)
{
	_ctx = ctx;
	_workerThr = std::thread(&transFormWorker::workerFnc,this);
	return;
}

void transFormWorker::workerFnc()
{
	
}
*/

void dec_capture_loop_fcn(void *arg)
{
	nvmpictx* ctx=(nvmpictx*)arg;
	NvVideoDecoder *dec = ctx->dec;
	
	struct v4l2_format v4l2Format;
	struct v4l2_crop v4l2Crop;
	struct v4l2_event v4l2Event;
	int ret;
	NVMPI_frameBuf* fb = NULL;
	//std::thread transformWorkersPool[3];

    /* Need to wait for the first Resolution change event, so that
       the decoder knows the stream resolution and can allocate appropriate
       buffers when we call REQBUFS. */
    do
    {
        /* Refer ioctl VIDIOC_DQEVENT */
        ret = dec->dqEvent(v4l2Event, 500);
        if (ret < 0)
        {
            if (errno == EAGAIN)
            {
                continue;
            }
            else
            {
               ERROR_MSG("Error in dequeueing decoder event");
               ctx->eos=true;
            }
        }
    }
    while ((v4l2Event.type != V4L2_EVENT_RESOLUTION_CHANGE) && !ctx->eos);

    /* Received the resolution change event, now can do respondToResolutionEvent. */
    if (!ctx->eos) respondToResolutionEvent(v4l2Format, v4l2Crop, ctx);
	
	while (!(ctx->eos || dec->isInError()))
	{
		NvBuffer *dec_buffer;
		
		// Check for Resolution change again.
		ret = dec->dqEvent(v4l2Event, false);
		if (ret == 0)
		{
			switch (v4l2Event.type)
			{
				case V4L2_EVENT_RESOLUTION_CHANGE:
					respondToResolutionEvent(v4l2Format, v4l2Crop, ctx);
					continue;
			}
		}
		
		/* Decoder capture loop */
		while(!ctx->eos)
		{
			struct v4l2_buffer v4l2_buf;
			struct v4l2_plane planes[MAX_PLANES];
			v4l2_buf.m.planes = planes;
			
			/* Dequeue a filled buffer. */
			if (dec->capture_plane.dqBuffer(v4l2_buf, &dec_buffer, NULL, 0))
			{
				if (errno == EAGAIN)
				{
					if (v4l2_buf.flags & V4L2_BUF_FLAG_LAST)
					{
						ERROR_MSG("Got EoS at capture plane");
						ctx->eos=true;
					}
					usleep(1000);
				}
				else
				{
					ERROR_MSG("Error while calling dequeue at capture plane");
					ctx->eos=true;
				}
				break;
			}
			
			dec_buffer->planes[0].fd = ctx->dmaBufferFileDescriptor[v4l2_buf.index];
			
			fb = ctx->framePool->dqEmptyBuf();
			
			if(fb)
			{
#ifdef WITH_NVUTILS
				ret = NvBufSurfTransform(ctx->dmaBufferSurface[v4l2_buf.index], fb->dst_dma_surface, &(ctx->transform_params));
#else
				ret = NvBufferTransform(dec_buffer->planes[0].fd, fb->dst_dma_fd, &(ctx->transform_params));
#endif
				TEST_ERROR(ret==-1, "Transform failed",ret);
				fb->timestamp = (v4l2_buf.timestamp.tv_usec % 1000000) + (v4l2_buf.timestamp.tv_sec * 1000000UL);
				
				ctx->framePool->qFilledBuf(fb);
			}
			else
			{
				printf("No empty buffers available to transform, Frame skipped!\n");
			}

			v4l2_buf.m.planes[0].m.fd = ctx->dmaBufferFileDescriptor[v4l2_buf.index];
			if (dec->capture_plane.qBuffer(v4l2_buf, NULL) < 0)
			{
				ERROR_MSG("Error while queueing buffer at decoder capture plane");
			}
		}
	}
	
#ifndef WITH_NVUTILS
	NvBufferSessionDestroy(ctx->session);
#endif
	
	return;
}

//TODO: accept in nvmpi_create_decoder stream params (width and height, etc...) from ffmpeg.
nvmpictx* nvmpi_create_decoder(nvCodingType codingType,nvPixFormat pixFormat){
	
	int ret;
	log_level = LOG_LEVEL_INFO;

	nvmpictx* ctx=new nvmpictx();

	ctx->dec = NvVideoDecoder::createVideoDecoder("dec0");
	TEST_ERROR(!ctx->dec, "Could not create decoder",ret);

	ret=ctx->dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE, 0, 0);
	TEST_ERROR(ret < 0, "Could not subscribe to V4L2_EVENT_RESOLUTION_CHANGE", ret);

	switch(codingType){
		case NV_VIDEO_CodingH264:
			ctx->decoder_pixfmt=V4L2_PIX_FMT_H264;
			break;
		case NV_VIDEO_CodingHEVC:
			ctx->decoder_pixfmt=V4L2_PIX_FMT_H265;
			break;
		case NV_VIDEO_CodingMPEG4:
			ctx->decoder_pixfmt=V4L2_PIX_FMT_MPEG4;
			break;
		case NV_VIDEO_CodingMPEG2:
			ctx->decoder_pixfmt=V4L2_PIX_FMT_MPEG2;
			break;
		case NV_VIDEO_CodingVP8:
			ctx->decoder_pixfmt=V4L2_PIX_FMT_VP8;
			break;
		case NV_VIDEO_CodingVP9:
			ctx->decoder_pixfmt=V4L2_PIX_FMT_VP9;
			break;
		default:
			ctx->decoder_pixfmt=V4L2_PIX_FMT_H264;
			break;
	}

	ret=ctx->dec->setOutputPlaneFormat(ctx->decoder_pixfmt, CHUNK_SIZE);

	TEST_ERROR(ret < 0, "Could not set output plane format", ret);

	ret = ctx->dec->setFrameInputMode(0);
	TEST_ERROR(ret < 0, "Error in decoder setFrameInputMode for NALU", ret);
	
	//TODO: create option to enable max performace mode (?)
	//ret = ctx->dec->setMaxPerfMode(true);
	//TEST_ERROR(ret < 0, "Error while setting decoder to max perf", ret);

	ret = ctx->dec->output_plane.setupPlane(V4L2_MEMORY_USERPTR, 10, false, true);
	TEST_ERROR(ret < 0, "Error while setting up output plane", ret);

	ctx->dec->output_plane.setStreamStatus(true);
	TEST_ERROR(ret < 0, "Error in output plane stream on", ret);

	ctx->out_pixfmt=pixFormat;
	ctx->framePool = new NVMPI_bufPool<NVMPI_frameBuf*>();
	ctx->eos=false;
	ctx->index=0;
	ctx->frame_size[0]=0;
	for(int index=0;index<MAX_BUFFERS;index++)
		ctx->dmaBufferFileDescriptor[index]=0;
	ctx->numberCaptureBuffers=0;
	ctx->dec_capture_loop = std::thread(dec_capture_loop_fcn,ctx);

	return ctx;
}

int nvmpi_decoder_put_packet(nvmpictx* ctx,nvPacket* packet)
{
	int ret;
	struct v4l2_buffer v4l2_buf;
	struct v4l2_plane planes[MAX_PLANES];
	NvBuffer *nvBuffer;

	memset(&v4l2_buf, 0, sizeof(v4l2_buf));
	memset(planes, 0, sizeof(planes));

	v4l2_buf.m.planes = planes;

	if (ctx->index < (int)ctx->dec->output_plane.getNumBuffers())
	{
		nvBuffer = ctx->dec->output_plane.getNthBuffer(ctx->index);
		v4l2_buf.index = ctx->index;
		ctx->index++;
	}
	else
	{
		ret = ctx->dec->output_plane.dqBuffer(v4l2_buf, &nvBuffer, NULL, -1);
		if (ret < 0)
		{
			cout << "Error DQing buffer at output plane" << std::endl;
			return false;
		}
	}

	memcpy(nvBuffer->planes[0].data,packet->payload,packet->payload_size);
	nvBuffer->planes[0].bytesused=packet->payload_size;
	v4l2_buf.m.planes[0].bytesused = nvBuffer->planes[0].bytesused;

	v4l2_buf.flags |= V4L2_BUF_FLAG_TIMESTAMP_COPY;
	v4l2_buf.timestamp.tv_sec = packet->pts / 1000000;
	v4l2_buf.timestamp.tv_usec = packet->pts % 1000000;

	ret = ctx->dec->output_plane.qBuffer(v4l2_buf, NULL);
	if (ret < 0)
	{
		std::cout << "Error Qing buffer at output plane" << std::endl;
		ctx->index--;
		return false;
	}

	if (v4l2_buf.m.planes[0].bytesused == 0)
	{
		ctx->eos=true;
		std::cout << "Input file read complete" << std::endl;
	}

	return 0;
}

int nvmpi_decoder_get_frame(nvmpictx* ctx,nvFrame* frame,bool wait)
{
	int ret;
	NVMPI_frameBuf* fb = ctx->framePool->dqFilledBuf();
	if(!fb) return -1;
	
#ifdef WITH_NVUTILS
	NvBufSurface *dSurf = fb->dst_dma_surface;
	ret=NvBufSurface2Raw(dSurf,0,0,ctx->frame_linesize[0],ctx->frame_height[0],frame->payload[0]);
	ret=NvBufSurface2Raw(dSurf,0,1,ctx->frame_linesize[1],ctx->frame_height[1],frame->payload[1]);
	if(ctx->out_pixfmt==NV_PIX_YUV420)
		ret=NvBufSurface2Raw(dSurf,0,2,ctx->frame_linesize[2],ctx->frame_height[2],frame->payload[2]);
#else
	int dFd = fb->dst_dma_fd;
	ret=NvBuffer2Raw(dFd,0,ctx->frame_linesize[0],ctx->frame_height[0],frame->payload[0]);
	ret=NvBuffer2Raw(dFd,1,ctx->frame_linesize[1],ctx->frame_height[1],frame->payload[1]);
	if(ctx->out_pixfmt==NV_PIX_YUV420)
		ret=NvBuffer2Raw(dFd,2,ctx->frame_linesize[2],ctx->frame_height[2],frame->payload[2]);
#endif
	
	//TODO handle NvBuffer2Raw fails
	
	frame->timestamp=fb->timestamp;
	//return buffer to pool
	ctx->framePool->qEmptyBuf(fb);
	
	return ret;
}

int nvmpi_decoder_close(nvmpictx* ctx)
{
	ctx->eos=true;
	ctx->dec->capture_plane.setStreamStatus(false);
	if (ctx->dec_capture_loop.joinable())
	{
		ctx->dec_capture_loop.join();
	}
	
	//deinit DstDmaBuffer and DecoderCapturePlane
	ctx->deinitDecoderCapturePlane();
	//empty frame queue and free buffers
	ctx->deinitFramePool();
	
	delete ctx->dec; ctx->dec = nullptr;

	delete ctx;

	return 0;
}


