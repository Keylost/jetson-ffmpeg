#include <nvmpi.h>
#include "avcodec.h"
#include "internal.h"
#include <stdio.h>
#include "libavutil/avstring.h"
#include "libavutil/avutil.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/mem.h"

#include "version.h"

//compatibility with ffmpeg 8.0+. FF_PROFILE renamed with AV_PROFILE
#if (LIBAVCODEC_VERSION_MAJOR >= 62 && LIBAVCODEC_VERSION_MINOR>= 11) || (LIBAVCODEC_VERSION_MAJOR >= 100)
#define FF_PROFILE_H264_INTRA AV_PROFILE_H264_INTRA
#define FF_PROFILE_UNKNOWN AV_PROFILE_UNKNOWN
#define FF_PROFILE_H264_HIGH AV_PROFILE_H264_HIGH
#define FF_PROFILE_H264_BASELINE AV_PROFILE_H264_BASELINE
#define FF_PROFILE_H264_MAIN AV_PROFILE_H264_MAIN
#endif

#if (LIBAVCODEC_VERSION_MAJOR >= 58 && LIBAVCODEC_VERSION_MINOR>= 134) || (LIBAVCODEC_VERSION_MAJOR >= 59)
#define NVMPI_FF_NEW_API
#include "encode.h"
#endif
#if (LIBAVCODEC_VERSION_MAJOR >= 60)
#include "codec_internal.h"
#endif

static const AVRational NVENC_TIMEBASE = {1, 1000000};

#define OPT_packet_pool_size_MIN 1
#define OPT_packet_pool_size_MAX 32
#define OPT_packet_pool_size_DEFAULT 5

typedef struct {
	const AVClass *class;
	nvmpictx* ctx;
	int num_capture_buffers;
	int packet_pool_size;
	int profile;
	int level;
	int rc;
	int preset;
	int encoder_flushing;
	AVFrame *frame; //tmp frame
}nvmpiEncodeContext;

nvPacket* nvmpienc_nvPacket_alloc(AVCodecContext *avctx, int bufSize);
void nvmpienc_nvPacket_free(nvPacket* nPkt);
int nvmpienc_nvPacket_reset(nvPacket* nPkt, AVCodecContext *avctx, int bufSize);
int nvmpienc_initPktPool(AVCodecContext *avctx, int pktNum);
int nvmpienc_deinitPktPool(AVCodecContext *avctx);

//alloc nvPacket and AVPacket buffer;
nvPacket* nvmpienc_nvPacket_alloc(AVCodecContext *avctx, int bufSize)
{
	AVPacket* pkt = av_packet_alloc();
	nvPacket* nPkt = (nvPacket*)malloc(sizeof(nvPacket));
	int res;
	memset(nPkt, 0, sizeof(nvPacket));
#if LIBAVCODEC_VERSION_MAJOR >= 60
	if((res = ff_get_encode_buffer(avctx, pkt, bufSize, 0)))
#else
	if((res = ff_alloc_packet2(avctx,pkt,bufSize,bufSize)))
#endif
	{
		av_packet_free(&pkt);
		free(nPkt);
		return NULL;
	}
	nPkt->privData = pkt;
	nPkt->payload = pkt->data;
	return nPkt;
}

void nvmpienc_nvPacket_free(nvPacket* nPkt)
{
	AVPacket* pkt = nPkt->privData;
	av_packet_free(&pkt);
	free(nPkt);
}

int nvmpienc_nvPacket_reset(nvPacket* nPkt, AVCodecContext *avctx, int bufSize)
{
	AVPacket* pkt = nPkt->privData;
	int res;
#if LIBAVCODEC_VERSION_MAJOR >= 60
	if((res = ff_get_encode_buffer(avctx, pkt, bufSize, 0)))
#else
	if((res = ff_alloc_packet2(avctx,pkt,bufSize,bufSize)))
#endif
	{
		return -1;
	}
	nPkt->payload = pkt->data;
	nPkt->payload_size = 0;
	nPkt->flags = 0;
	nPkt->pts = 0;
	return 0;
}

//must call after nvmpi_create_encoder() to preallocate buffers
int nvmpienc_initPktPool(AVCodecContext *avctx, int pktNum)
{
	nvmpiEncodeContext * nvmpi_context = avctx->priv_data;
	//TODO free allocated mem on error
	for(int i=0;i<pktNum;i++)
	{
		nvPacket* nPkt = nvmpienc_nvPacket_alloc(avctx, NVMPI_ENC_CHUNK_SIZE);
		nvmpi_encoder_qEmptyPacket(nvmpi_context->ctx, nPkt);
	}
	return 0;
}

//must call before nvmpi_encoder_close() too free buffers memory
int nvmpienc_deinitPktPool(AVCodecContext *avctx)
{
	nvmpiEncodeContext * nvmpi_context = avctx->priv_data;
	nvmpictx *ctx = nvmpi_context->ctx;
	nvPacket* nPkt;
	
	while(nvmpi_encoder_dqEmptyPacket(ctx, &nPkt) == 0)
	{
		AVPacket* pkt = nPkt->privData;
		av_packet_free(&pkt);
		free(nPkt);
	}
	while(nvmpi_encoder_get_packet(ctx, &nPkt) == 0)
	{
		AVPacket* pkt = nPkt->privData;
		av_packet_free(&pkt);
		free(nPkt);
	}
	
	//TODO check that all mem returned to nothing
	return 0;
}

static av_cold int nvmpi_encode_init(AVCodecContext *avctx)
{
	nvmpiEncodeContext * nvmpi_context = avctx->priv_data;

	nvEncParam param={0};

	param.width=avctx->width;
	param.height=avctx->height;
	param.bitrate=avctx->bit_rate;
	param.vbv_buffer_size = avctx->rc_buffer_size;
	//TODO use rc_initial_buffer_occupancy or ignore?
	param.mode_vbr=0;
	param.idr_interval=60;
	param.iframe_interval=30;
	param.peak_bitrate=avctx->rc_max_rate;
	param.fps_n=avctx->framerate.num;
	param.fps_d=avctx->framerate.den;
	param.profile=nvmpi_context->profile& ~FF_PROFILE_H264_INTRA;
	param.level=nvmpi_context->level;
	param.capture_num=nvmpi_context->num_capture_buffers;
	//param.packet_pool_size=nvmpi_context->packet_pool_size;
	param.hw_preset_type=nvmpi_context->preset;
	param.insert_spspps_idr=(avctx->flags & AV_CODEC_FLAG_GLOBAL_HEADER)?0:1;
	
	nvmpi_context->frame = av_frame_alloc();
	if (!nvmpi_context->frame) return AVERROR(ENOMEM);

	if(nvmpi_context->rc==1){
		param.mode_vbr=1;
	}

	if(avctx->qmin >= 0 && avctx->qmax >= 0){
		param.qmin=avctx->qmin;
		param.qmax=avctx->qmax;
	}

	if (avctx->refs >= 0){
		param.refs=avctx->refs;

	}

	if(avctx->max_b_frames > 0 && avctx->max_b_frames < 3){
		param.max_b_frames=avctx->max_b_frames;
	}

	if(avctx->gop_size>0){
		param.idr_interval=param.iframe_interval=avctx->gop_size;

	}

	//TODO should replace it. must gen extradata directly without calling for encoder
	if ((avctx->flags & AV_CODEC_FLAG_GLOBAL_HEADER) && (avctx->codec->id == AV_CODEC_ID_H264 || avctx->codec->id == AV_CODEC_ID_H265))
	{
		uint8_t *dst[4];
		int linesize[4];
		nvFrame _nvframe={0};
		nvPacket *nPkt;
		nvmpictx *_ctx;
		int i;
		int ret;
		int64_t shiftPts = 1000000/param.fps_n;
		if(avctx->codec->id == AV_CODEC_ID_H264) param.codingType = NV_VIDEO_CodingH264;
		else param.codingType = NV_VIDEO_CodingHEVC;
		av_image_alloc(dst, linesize,avctx->width,avctx->height,avctx->pix_fmt,1);

		nvmpi_context->ctx = nvmpi_create_encoder(&param);
		_ctx = nvmpi_context->ctx;
		//TODO error handling. if(!_ctx)
		nvmpienc_initPktPool(avctx,nvmpi_context->packet_pool_size);
		i=0;
		_nvframe.timestamp=0;

		while(true)
		{
			_nvframe.payload[0]=dst[0];
			_nvframe.payload[1]=dst[1];
			_nvframe.payload[2]=dst[2];
			_nvframe.payload_size[0]=linesize[0]*avctx->height;
			_nvframe.payload_size[1]=linesize[1]*avctx->height/2;
			_nvframe.payload_size[2]=linesize[2]*avctx->height/2;
			_nvframe.linesize[0]=linesize[0];
			_nvframe.linesize[1]=linesize[1];
			_nvframe.linesize[2]=linesize[2];
			_nvframe.timestamp+=shiftPts;

			nvmpi_encoder_put_frame(_ctx,&_nvframe);

			ret=nvmpi_encoder_get_packet(_ctx,&nPkt);

			if(ret<0)
				continue;

			//find idr index
			while(i<nPkt->payload_size)
			{
				//check if nal start code
				if(nPkt->payload[i] == 0 && nPkt->payload[i+1] == 0 && nPkt->payload[i+2] == 0 && nPkt->payload[i+3] == 0x01)
				{
					if(param.codingType == NV_VIDEO_CodingH264)
					{
						//h264 idr
						if(nPkt->payload[i+4] == 0x65) break;
					}
					else // NV_VIDEO_CodingHEVC:
					{
						//h265 idr
						if(nPkt->payload[i+4] == 0x26 || nPkt->payload[i+4] == 0x28) break;
					}
				}
				i++;
			}

			avctx->extradata_size=i;
			avctx->extradata	= av_mallocz( avctx->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE );
			memcpy( avctx->extradata, nPkt->payload,avctx->extradata_size);
			memset( avctx->extradata + avctx->extradata_size, 0, AV_INPUT_BUFFER_PADDING_SIZE );
			
			nvmpienc_nvPacket_free(nPkt);
			nPkt = nvmpienc_nvPacket_alloc(avctx, NVMPI_ENC_CHUNK_SIZE);
			
			//return buffer to pool
			nvmpi_encoder_qEmptyPacket(nvmpi_context->ctx, nPkt);
			//send eos
			nvmpi_encoder_put_frame(nvmpi_context->ctx,NULL);
			
			break;
		}
		
		//drain encoder
		while(true)
		{
			ret=nvmpi_encoder_get_packet(_ctx,&nPkt);
			if(ret < 0)
			{
				if(ret == -2) break; //got eos
				//usleep(1000);
				continue;
			}
			nvmpienc_nvPacket_free(nPkt);
			nPkt = nvmpienc_nvPacket_alloc(avctx, NVMPI_ENC_CHUNK_SIZE);
			nvmpi_encoder_qEmptyPacket(nvmpi_context->ctx, nPkt);
		}
		
		av_freep(&dst[0]); //free allocated image planes
		nvmpienc_deinitPktPool(avctx);
		nvmpi_encoder_close(nvmpi_context->ctx);
		nvmpi_context->ctx = NULL;
	}

	if(avctx->codec->id == AV_CODEC_ID_H264)
	{
		param.codingType = NV_VIDEO_CodingH264;
		nvmpi_context->ctx=nvmpi_create_encoder(&param);
	}
	else if(avctx->codec->id == AV_CODEC_ID_HEVC)
	{
		param.codingType = NV_VIDEO_CodingHEVC;
		nvmpi_context->ctx=nvmpi_create_encoder(&param);
	}
	//else TODO
	
	if(nvmpi_context->ctx)
	{
		nvmpienc_initPktPool(avctx,nvmpi_context->packet_pool_size);
	}
	//TODO error handling. if(!nvmpi_context->ctx)

	return 0;
}

static int ff_nvmpi_send_frame(AVCodecContext *avctx,const AVFrame *frame)
{
	nvmpiEncodeContext * nvmpi_context = avctx->priv_data;
	nvFrame _nvframe={0};
	int res;

	if (nvmpi_context->encoder_flushing)
		return AVERROR_EOF;

	if(frame)
	{
		_nvframe.payload[0]=frame->data[0];
		_nvframe.payload[1]=frame->data[1];
		_nvframe.payload[2]=frame->data[2];

		_nvframe.payload_size[0]=frame->linesize[0]*frame->height;
		_nvframe.payload_size[1]=frame->linesize[1]*frame->height/2;
		_nvframe.payload_size[2]=frame->linesize[2]*frame->height/2;

		_nvframe.linesize[0]=frame->linesize[0];
		_nvframe.linesize[1]=frame->linesize[1];
		_nvframe.linesize[2]=frame->linesize[2];

		//_nvframe.timestamp=frame->pts;
		_nvframe.timestamp=av_rescale_q(frame->pts, avctx->time_base, NVENC_TIMEBASE);
		//_nvframe.timestamp=frame->pts*avctx->time_base.num*1000*1000/avctx->time_base.den;

		res=nvmpi_encoder_put_frame(nvmpi_context->ctx,&_nvframe);

		if(res<0)
			return res;
	}
	else
	{
		nvmpi_context->encoder_flushing = 1;
		nvmpi_encoder_put_frame(nvmpi_context->ctx,NULL);
	}

	return 0;
}

static int ff_nvmpi_receive_packet(AVCodecContext *avctx, AVPacket *pkt)
{
	nvmpiEncodeContext * nvmpi_context = avctx->priv_data;
	nvPacket *nPkt;
	AVPacket *aPkt;
	int res;
	
	res = nvmpi_encoder_get_packet(nvmpi_context->ctx,&nPkt);
	if(res<0)
	{
		//If the encoder is in flushing state, then get_packet will block and return either a packet or EOF
		if(nvmpi_context->encoder_flushing) return AVERROR_EOF;
		return AVERROR(EAGAIN); //nvmpi get_packet returns -1 if no packets are pending
	}
	
	aPkt = (AVPacket*)(nPkt->privData);
	//aPkt->dts=aPkt->pts=nPkt->pts;
	aPkt->dts = aPkt->pts = av_rescale_q(nPkt->pts, NVENC_TIMEBASE, avctx->time_base);
	av_shrink_packet(aPkt, nPkt->payload_size);
	if(nPkt->flags& AV_PKT_FLAG_KEY) aPkt->flags = AV_PKT_FLAG_KEY;
	av_packet_move_ref(pkt, aPkt);
	
	if(nvmpienc_nvPacket_reset(nPkt, avctx, NVMPI_ENC_CHUNK_SIZE))
	{
		nvmpienc_nvPacket_free(nPkt);
		return AVERROR(ENOMEM);
	}
	
	nvmpi_encoder_qEmptyPacket(nvmpi_context->ctx, nPkt);

	return 0;
}

/*
 * //old avcodec .encode2 api. TODO: remove it
static int ff_nvmpi_encode_frame(AVCodecContext *avctx, AVPacket *pkt,const AVFrame *frame, int *got_packet)
{
	int res;
	*got_packet = 0;

	res = ff_nvmpi_send_frame(avctx, frame);

	if(res < 0)
	{
		if(res != AVERROR_EOF && res != AVERROR(EAGAIN)) return res;
	}

	res = ff_nvmpi_receive_packet(avctx, pkt);
	if(res<0) return res;
	
	*got_packet = 1;

	return 0;
}
*/

#ifdef NVMPI_FF_NEW_API
static int ff_nvmpi_receive_packet_async(AVCodecContext *avctx, AVPacket *pkt)
{
	int res;
	nvmpiEncodeContext * nvmpi_context = avctx->priv_data;
	AVFrame *frame = nvmpi_context->frame;
	bool needSendFrame = true;
	
	if (!frame->buf[0])
	{
		res = ff_encode_get_frame(avctx, frame);
		if (res < 0)
		{
			if(res == AVERROR(EAGAIN)) needSendFrame = false;
			else if(res == AVERROR_EOF) frame = NULL;
			else return res;
		}
	}
	
	if(needSendFrame)
	{
		res = ff_nvmpi_send_frame(avctx, frame);
		if (res < 0)
		{
			if(res != AVERROR_EOF && res != AVERROR(EAGAIN)) return res;
		}
		else av_frame_unref(frame);
	}

	res = ff_nvmpi_receive_packet(avctx, pkt);
	if(res<0) return res;

	return 0;
}
#endif

static av_cold int nvmpi_encode_close(AVCodecContext *avctx)
{
	nvmpiEncodeContext *nvmpi_context = avctx->priv_data;
	
	//drain encoder
	{
		int ret;
		nvPacket *nPkt;
		if(!nvmpi_context->encoder_flushing)
		{
			nvmpi_context->encoder_flushing = 1;
			nvmpi_encoder_put_frame(nvmpi_context->ctx,NULL);
		}
		
		while(1)
		{
			ret=nvmpi_encoder_get_packet(nvmpi_context->ctx,&nPkt);
			if(ret < 0)
			{
				if(ret == -2) break; //got eos
				//usleep(1000);
				continue;
			}
			nvmpienc_nvPacket_free(nPkt);
			nPkt = nvmpienc_nvPacket_alloc(avctx, NVMPI_ENC_CHUNK_SIZE);
			nvmpi_encoder_qEmptyPacket(nvmpi_context->ctx, nPkt);
		}
	}
	
	nvmpienc_deinitPktPool(avctx);
	nvmpi_encoder_close(nvmpi_context->ctx);
	av_frame_free(&nvmpi_context->frame);

	return 0;
}

#if LIBAVCODEC_VERSION_MAJOR >= 60
static const FFCodecDefault defaults[] = {
#else
static const AVCodecDefault defaults[] = {
#endif
	{ "b", "2M" },
	{ "qmin", "-1" },
	{ "qmax", "-1" },
	{ "qdiff", "-1" },
	{ "qblur", "-1" },
	{ "qcomp", "-1" },
	{ "g", "50" },
	{ "bf", "0" },
	{ "refs", "0" },
	{ NULL },
};


#define OFFSET(x) offsetof(nvmpiEncodeContext, x)
#define VE AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM

static const AVOption options[] = {
	{ "num_capture_buffers", "Number of buffers in the capture context", OFFSET(num_capture_buffers), AV_OPT_TYPE_INT, {.i64 = 10 }, 1, 32, VE, "num_capture_buffers" },
	{ "packet_pool_size", "Number of packets that could be buffered in the encoder before user must read it with avcodec_receive_packet()", OFFSET(packet_pool_size), AV_OPT_TYPE_INT, {.i64 = OPT_packet_pool_size_DEFAULT }, OPT_packet_pool_size_MIN, OPT_packet_pool_size_MAX, VE, "packet_pool_size" },
	/// Profile,

	{ "profile",  "Set the encoding profile", OFFSET(profile), AV_OPT_TYPE_INT,   { .i64 = FF_PROFILE_UNKNOWN },       FF_PROFILE_UNKNOWN, FF_PROFILE_H264_HIGH, VE, "profile" },
	{ "baseline", "",                         0,               AV_OPT_TYPE_CONST, { .i64 = FF_PROFILE_H264_BASELINE }, 0, 0, VE, "profile" },
	{ "main",     "",                         0,               AV_OPT_TYPE_CONST, { .i64 = FF_PROFILE_H264_MAIN },     0, 0, VE, "profile" },
	{ "high",     "",                         0,               AV_OPT_TYPE_CONST, { .i64 = FF_PROFILE_H264_HIGH },     0, 0, VE, "profile" },

	/// Profile Level
	{ "level",          "Profile Level",        OFFSET(level),  AV_OPT_TYPE_INT,   { .i64 = 0  }, 0, 62, VE, "level" },
	{ "auto",           "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 0  }, 0, 0,  VE, "level" },
	{ "1.0",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 10 }, 0, 0,  VE, "level" },
	{ "1.1",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 11 }, 0, 0,  VE, "level" },
	{ "1.2",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 12 }, 0, 0,  VE, "level" },
	{ "1.3",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 13 }, 0, 0,  VE, "level" },
	{ "2.0",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 20 }, 0, 0,  VE, "level" },
	{ "2.1",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 21 }, 0, 0,  VE, "level" },
	{ "2.2",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 22 }, 0, 0,  VE, "level" },
	{ "3.0",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 30 }, 0, 0,  VE, "level" },
	{ "3.1",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 31 }, 0, 0,  VE, "level" },
	{ "3.2",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 32 }, 0, 0,  VE, "level" },
	{ "4.0",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 40 }, 0, 0,  VE, "level" },
	{ "4.1",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 41 }, 0, 0,  VE, "level" },
	{ "4.2",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 42 }, 0, 0,  VE, "level" },
	{ "5.0",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 50 }, 0, 0,  VE, "level" },
	{ "5.1",            "",                     0,              AV_OPT_TYPE_CONST, { .i64 = 51 }, 0, 0,  VE, "level" },

	{ "rc",           "Override the preset rate-control",   OFFSET(rc),           AV_OPT_TYPE_INT,   { .i64 = -1 },                                  -1, INT_MAX, VE, "rc" },
	{ "cbr",          "Constant bitrate mode",              0,                    AV_OPT_TYPE_CONST, { .i64 = 0 },                       0, 0, VE, "rc" },
	{ "vbr",          "Variable bitrate mode",              0,                    AV_OPT_TYPE_CONST, { .i64 = 1 },                       0, 0, VE, "rc" },

	{ "preset",          "Set the encoding preset",            OFFSET(preset),       AV_OPT_TYPE_INT,   { .i64 = 3 }, 1, 4, VE, "preset" },
	{ "default",         "",                                   0,                    AV_OPT_TYPE_CONST, { .i64 = 3 }, 0, 0, VE, "preset" },
	{ "slow",            "",                        0,                    AV_OPT_TYPE_CONST, { .i64 = 4 },            0, 0, VE, "preset" },
	{ "medium",          "",                        0,                    AV_OPT_TYPE_CONST, { .i64 = 3 },            0, 0, VE, "preset" },
	{ "fast",            "",                        0,                    AV_OPT_TYPE_CONST, { .i64 = 2 },            0, 0, VE, "preset" },
	{ "ultrafast",       "",                        0,                    AV_OPT_TYPE_CONST, { .i64 = 1 },            0, 0, VE, "preset" },
	{ NULL }
};


#define NVMPI_ENC_CLASS(NAME) \
	static const AVClass nvmpi_ ## NAME ## _enc_class = { \
		.class_name = #NAME "_nvmpi_encoder", \
		.item_name  = av_default_item_name, \
		.option     = options, \
		.version    = LIBAVUTIL_VERSION_INT, \
	};


#if LIBAVCODEC_VERSION_MAJOR >= 60
	#define NVMPI_ENC(NAME, LONGNAME, CODEC) \
		NVMPI_ENC_CLASS(NAME) \
		FFCodec ff_ ## NAME ## _nvmpi_encoder = { \
			.p.name           = #NAME "_nvmpi" , \
			CODEC_LONG_NAME("nvmpi " LONGNAME " encoder wrapper"), \
			.p.type           = AVMEDIA_TYPE_VIDEO, \
			.p.id             = CODEC , \
			.priv_data_size = sizeof(nvmpiEncodeContext), \
			.p.priv_class     = &nvmpi_ ## NAME ##_enc_class, \
			.init           = nvmpi_encode_init, \
			FF_CODEC_RECEIVE_PACKET_CB(ff_nvmpi_receive_packet_async), \
			.close          = nvmpi_encode_close, \
			.p.pix_fmts       = (const enum AVPixelFormat[]) { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE },\
			.p.capabilities   = AV_CODEC_CAP_HARDWARE | AV_CODEC_CAP_DELAY, \
			.defaults       = defaults,\
			.p.wrapper_name   = "nvmpi", \
		};
#else
	#ifdef NVMPI_FF_NEW_API
		#define NVMPI_ENC_API_CALLS \
				.receive_packet = ff_nvmpi_receive_packet_async
	#else
		#define NVMPI_ENC_API_CALLS \
				.send_frame     = ff_nvmpi_send_frame, \
				.receive_packet = ff_nvmpi_receive_packet
	#endif
	
	#define NVMPI_ENC(NAME, LONGNAME, CODEC) \
		NVMPI_ENC_CLASS(NAME) \
		AVCodec ff_ ## NAME ## _nvmpi_encoder = { \
			.name           = #NAME "_nvmpi" , \
			.long_name      = NULL_IF_CONFIG_SMALL("nvmpi " LONGNAME " encoder wrapper"), \
			.type           = AVMEDIA_TYPE_VIDEO, \
			.id             = CODEC , \
			.priv_data_size = sizeof(nvmpiEncodeContext), \
			.priv_class     = &nvmpi_ ## NAME ##_enc_class, \
			.init           = nvmpi_encode_init, \
			NVMPI_ENC_API_CALLS, \
			.close          = nvmpi_encode_close, \
			.pix_fmts       = (const enum AVPixelFormat[]) { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE },\
			.capabilities   = AV_CODEC_CAP_HARDWARE | AV_CODEC_CAP_DELAY, \
			.defaults       = defaults,\
			.wrapper_name   = "nvmpi", \
		};
#endif

NVMPI_ENC(h264, "H.264", AV_CODEC_ID_H264);
NVMPI_ENC(hevc, "HEVC", AV_CODEC_ID_HEVC);
