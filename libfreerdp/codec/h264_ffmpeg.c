/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 Bitmap Compression
 *
 * Copyright 2015 Marc-André Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Mike McDonald <Mike.McDonald@software.dell.com>
 * Copyright 2014 erbth <t.erbesdobler@team103.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <freerdp/config.h>

#include <winpr/wlog.h>
#include <winpr/json.h>

#include <freerdp/log.h>
#include <freerdp/codec/h264.h>
#include <freerdp/utils/helpers.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

#include "h264.h"

#ifdef WITH_FFMPEG_HWACCEL
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(55, 9, 0)
#include <libavutil/hwcontext.h>
#else
#pragma warning You have asked for VA - API decoding, \
    but your version of libavutil is too old !Disabling.
#undef WITH_FFMPEG_HWACCEL
#endif
#endif

/* Fallback support for older libavcodec versions */
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 59, 100)
#define AV_CODEC_ID_H264 CODEC_ID_H264
#endif

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(56, 34, 2)
#define AV_CODEC_FLAG_LOOP_FILTER CODEC_FLAG_LOOP_FILTER
#define AV_CODEC_CAP_TRUNCATED CODEC_CAP_TRUNCATED
#define AV_CODEC_FLAG_TRUNCATED CODEC_FLAG_TRUNCATED
#endif

#if LIBAVUTIL_VERSION_MAJOR < 52
#define AV_PIX_FMT_YUV420P PIX_FMT_YUV420P
#endif

#if defined(WITH_FFMPEG_HWACCEL)
static const char* av_format_str(int32_t format)
{
#define EVCASE(x) \
	case x:       \
		return #x

	switch (format)
	{
		EVCASE(AV_PIX_FMT_NONE);
		EVCASE(AV_PIX_FMT_YUV420P);
		EVCASE(AV_PIX_FMT_YUYV422);
		EVCASE(AV_PIX_FMT_RGB24);
		EVCASE(AV_PIX_FMT_BGR24);
		EVCASE(AV_PIX_FMT_YUV422P);
		EVCASE(AV_PIX_FMT_YUV444P);
		EVCASE(AV_PIX_FMT_YUV410P);
		EVCASE(AV_PIX_FMT_YUV411P);
		EVCASE(AV_PIX_FMT_GRAY8);
		EVCASE(AV_PIX_FMT_MONOWHITE);
		EVCASE(AV_PIX_FMT_MONOBLACK);
		EVCASE(AV_PIX_FMT_PAL8);
		EVCASE(AV_PIX_FMT_YUVJ420P);
		EVCASE(AV_PIX_FMT_YUVJ422P);
		EVCASE(AV_PIX_FMT_YUVJ444P);
		EVCASE(AV_PIX_FMT_UYVY422);
		EVCASE(AV_PIX_FMT_UYYVYY411);
		EVCASE(AV_PIX_FMT_BGR8);
		EVCASE(AV_PIX_FMT_BGR4);
		EVCASE(AV_PIX_FMT_BGR4_BYTE);
		EVCASE(AV_PIX_FMT_RGB8);
		EVCASE(AV_PIX_FMT_RGB4);
		EVCASE(AV_PIX_FMT_RGB4_BYTE);
		EVCASE(AV_PIX_FMT_NV12);
		EVCASE(AV_PIX_FMT_NV21);
		EVCASE(AV_PIX_FMT_ARGB);
		EVCASE(AV_PIX_FMT_RGBA);
		EVCASE(AV_PIX_FMT_ABGR);
		EVCASE(AV_PIX_FMT_BGRA);
		EVCASE(AV_PIX_FMT_GRAY16BE);
		EVCASE(AV_PIX_FMT_GRAY16LE);
		EVCASE(AV_PIX_FMT_YUV440P);
		EVCASE(AV_PIX_FMT_YUVJ440P);
		EVCASE(AV_PIX_FMT_YUVA420P);
		EVCASE(AV_PIX_FMT_RGB48BE);
		EVCASE(AV_PIX_FMT_RGB48LE);
		EVCASE(AV_PIX_FMT_RGB565BE);
		EVCASE(AV_PIX_FMT_RGB565LE);
		EVCASE(AV_PIX_FMT_RGB555BE);
		EVCASE(AV_PIX_FMT_RGB555LE);
		EVCASE(AV_PIX_FMT_BGR565BE);
		EVCASE(AV_PIX_FMT_BGR565LE);
		EVCASE(AV_PIX_FMT_BGR555BE);
		EVCASE(AV_PIX_FMT_BGR555LE);
		EVCASE(AV_PIX_FMT_VAAPI);
		EVCASE(AV_PIX_FMT_YUV420P16LE);
		EVCASE(AV_PIX_FMT_YUV420P16BE);
		EVCASE(AV_PIX_FMT_YUV422P16LE);
		EVCASE(AV_PIX_FMT_YUV422P16BE);
		EVCASE(AV_PIX_FMT_YUV444P16LE);
		EVCASE(AV_PIX_FMT_YUV444P16BE);
		EVCASE(AV_PIX_FMT_DXVA2_VLD);
		EVCASE(AV_PIX_FMT_RGB444LE);
		EVCASE(AV_PIX_FMT_RGB444BE);
		EVCASE(AV_PIX_FMT_BGR444LE);
		EVCASE(AV_PIX_FMT_BGR444BE);
		EVCASE(AV_PIX_FMT_GRAY8A);
		EVCASE(AV_PIX_FMT_BGR48BE);
		EVCASE(AV_PIX_FMT_BGR48LE);
		EVCASE(AV_PIX_FMT_YUV420P9BE);
		EVCASE(AV_PIX_FMT_YUV420P9LE);
		EVCASE(AV_PIX_FMT_YUV420P10BE);
		EVCASE(AV_PIX_FMT_YUV420P10LE);
		EVCASE(AV_PIX_FMT_YUV422P10BE);
		EVCASE(AV_PIX_FMT_YUV422P10LE);
		EVCASE(AV_PIX_FMT_YUV444P9BE);
		EVCASE(AV_PIX_FMT_YUV444P9LE);
		EVCASE(AV_PIX_FMT_YUV444P10BE);
		EVCASE(AV_PIX_FMT_YUV444P10LE);
		EVCASE(AV_PIX_FMT_YUV422P9BE);
		EVCASE(AV_PIX_FMT_YUV422P9LE);
		EVCASE(AV_PIX_FMT_GBR24P);
		EVCASE(AV_PIX_FMT_GBRP9BE);
		EVCASE(AV_PIX_FMT_GBRP9LE);
		EVCASE(AV_PIX_FMT_GBRP10BE);
		EVCASE(AV_PIX_FMT_GBRP10LE);
		EVCASE(AV_PIX_FMT_GBRP16BE);
		EVCASE(AV_PIX_FMT_GBRP16LE);
		EVCASE(AV_PIX_FMT_YUVA422P);
		EVCASE(AV_PIX_FMT_YUVA444P);
		EVCASE(AV_PIX_FMT_YUVA420P9BE);
		EVCASE(AV_PIX_FMT_YUVA420P9LE);
		EVCASE(AV_PIX_FMT_YUVA422P9BE);
		EVCASE(AV_PIX_FMT_YUVA422P9LE);
		EVCASE(AV_PIX_FMT_YUVA444P9BE);
		EVCASE(AV_PIX_FMT_YUVA444P9LE);
		EVCASE(AV_PIX_FMT_YUVA420P10BE);
		EVCASE(AV_PIX_FMT_YUVA420P10LE);
		EVCASE(AV_PIX_FMT_YUVA422P10BE);
		EVCASE(AV_PIX_FMT_YUVA422P10LE);
		EVCASE(AV_PIX_FMT_YUVA444P10BE);
		EVCASE(AV_PIX_FMT_YUVA444P10LE);
		EVCASE(AV_PIX_FMT_YUVA420P16BE);
		EVCASE(AV_PIX_FMT_YUVA420P16LE);
		EVCASE(AV_PIX_FMT_YUVA422P16BE);
		EVCASE(AV_PIX_FMT_YUVA422P16LE);
		EVCASE(AV_PIX_FMT_YUVA444P16BE);
		EVCASE(AV_PIX_FMT_YUVA444P16LE);
		EVCASE(AV_PIX_FMT_VDPAU);
		EVCASE(AV_PIX_FMT_XYZ12LE);
		EVCASE(AV_PIX_FMT_XYZ12BE);
		EVCASE(AV_PIX_FMT_NV16);
		EVCASE(AV_PIX_FMT_NV20LE);
		EVCASE(AV_PIX_FMT_NV20BE);
		EVCASE(AV_PIX_FMT_RGBA64BE);
		EVCASE(AV_PIX_FMT_RGBA64LE);
		EVCASE(AV_PIX_FMT_BGRA64BE);
		EVCASE(AV_PIX_FMT_BGRA64LE);
		EVCASE(AV_PIX_FMT_YVYU422);
		EVCASE(AV_PIX_FMT_YA16BE);
		EVCASE(AV_PIX_FMT_YA16LE);
		EVCASE(AV_PIX_FMT_GBRAP);
		EVCASE(AV_PIX_FMT_GBRAP16BE);
		EVCASE(AV_PIX_FMT_GBRAP16LE);
		EVCASE(AV_PIX_FMT_QSV);
		EVCASE(AV_PIX_FMT_MMAL);
		EVCASE(AV_PIX_FMT_D3D11VA_VLD);
		EVCASE(AV_PIX_FMT_CUDA);
		EVCASE(AV_PIX_FMT_0RGB);
		EVCASE(AV_PIX_FMT_RGB0);
		EVCASE(AV_PIX_FMT_0BGR);
		EVCASE(AV_PIX_FMT_BGR0);
		EVCASE(AV_PIX_FMT_YUV420P12BE);
		EVCASE(AV_PIX_FMT_YUV420P12LE);
		EVCASE(AV_PIX_FMT_YUV420P14BE);
		EVCASE(AV_PIX_FMT_YUV420P14LE);
		EVCASE(AV_PIX_FMT_YUV422P12BE);
		EVCASE(AV_PIX_FMT_YUV422P12LE);
		EVCASE(AV_PIX_FMT_YUV422P14BE);
		EVCASE(AV_PIX_FMT_YUV422P14LE);
		EVCASE(AV_PIX_FMT_YUV444P12BE);
		EVCASE(AV_PIX_FMT_YUV444P12LE);
		EVCASE(AV_PIX_FMT_YUV444P14BE);
		EVCASE(AV_PIX_FMT_YUV444P14LE);
		EVCASE(AV_PIX_FMT_GBRP12BE);
		EVCASE(AV_PIX_FMT_GBRP12LE);
		EVCASE(AV_PIX_FMT_GBRP14BE);
		EVCASE(AV_PIX_FMT_GBRP14LE);
		EVCASE(AV_PIX_FMT_YUVJ411P);
		EVCASE(AV_PIX_FMT_BAYER_BGGR8);
		EVCASE(AV_PIX_FMT_BAYER_RGGB8);
		EVCASE(AV_PIX_FMT_BAYER_GBRG8);
		EVCASE(AV_PIX_FMT_BAYER_GRBG8);
		EVCASE(AV_PIX_FMT_BAYER_BGGR16LE);
		EVCASE(AV_PIX_FMT_BAYER_BGGR16BE);
		EVCASE(AV_PIX_FMT_BAYER_RGGB16LE);
		EVCASE(AV_PIX_FMT_BAYER_RGGB16BE);
		EVCASE(AV_PIX_FMT_BAYER_GBRG16LE);
		EVCASE(AV_PIX_FMT_BAYER_GBRG16BE);
		EVCASE(AV_PIX_FMT_BAYER_GRBG16LE);
		EVCASE(AV_PIX_FMT_BAYER_GRBG16BE);
		EVCASE(AV_PIX_FMT_YUV440P10LE);
		EVCASE(AV_PIX_FMT_YUV440P10BE);
		EVCASE(AV_PIX_FMT_YUV440P12LE);
		EVCASE(AV_PIX_FMT_YUV440P12BE);
		EVCASE(AV_PIX_FMT_AYUV64LE);
		EVCASE(AV_PIX_FMT_AYUV64BE);
		EVCASE(AV_PIX_FMT_VIDEOTOOLBOX);
		EVCASE(AV_PIX_FMT_P010LE);
		EVCASE(AV_PIX_FMT_P010BE);
		EVCASE(AV_PIX_FMT_GBRAP12BE);
		EVCASE(AV_PIX_FMT_GBRAP12LE);
		EVCASE(AV_PIX_FMT_GBRAP10BE);
		EVCASE(AV_PIX_FMT_GBRAP10LE);
		EVCASE(AV_PIX_FMT_MEDIACODEC);
		EVCASE(AV_PIX_FMT_GRAY12BE);
		EVCASE(AV_PIX_FMT_GRAY12LE);
		EVCASE(AV_PIX_FMT_GRAY10BE);
		EVCASE(AV_PIX_FMT_GRAY10LE);
		EVCASE(AV_PIX_FMT_P016LE);
		EVCASE(AV_PIX_FMT_P016BE);
		EVCASE(AV_PIX_FMT_D3D11);
		EVCASE(AV_PIX_FMT_GRAY9BE);
		EVCASE(AV_PIX_FMT_GRAY9LE);
		EVCASE(AV_PIX_FMT_GBRPF32BE);
		EVCASE(AV_PIX_FMT_GBRPF32LE);
		EVCASE(AV_PIX_FMT_GBRAPF32BE);
		EVCASE(AV_PIX_FMT_GBRAPF32LE);
		EVCASE(AV_PIX_FMT_DRM_PRIME);
		EVCASE(AV_PIX_FMT_OPENCL);
		EVCASE(AV_PIX_FMT_GRAY14BE);
		EVCASE(AV_PIX_FMT_GRAY14LE);
		EVCASE(AV_PIX_FMT_GRAYF32BE);
		EVCASE(AV_PIX_FMT_GRAYF32LE);
		EVCASE(AV_PIX_FMT_YUVA422P12BE);
		EVCASE(AV_PIX_FMT_YUVA422P12LE);
		EVCASE(AV_PIX_FMT_YUVA444P12BE);
		EVCASE(AV_PIX_FMT_YUVA444P12LE);
		EVCASE(AV_PIX_FMT_NV24);
		EVCASE(AV_PIX_FMT_NV42);
		EVCASE(AV_PIX_FMT_VULKAN);
		EVCASE(AV_PIX_FMT_Y210BE);
		EVCASE(AV_PIX_FMT_Y210LE);
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 17, 100)
		EVCASE(AV_PIX_FMT_P210BE);
		EVCASE(AV_PIX_FMT_P210LE);
		EVCASE(AV_PIX_FMT_P216BE);
		EVCASE(AV_PIX_FMT_P216LE);
		EVCASE(AV_PIX_FMT_P410BE);
		EVCASE(AV_PIX_FMT_P410LE);
		EVCASE(AV_PIX_FMT_X2RGB10LE);
		EVCASE(AV_PIX_FMT_X2RGB10BE);
		EVCASE(AV_PIX_FMT_X2BGR10LE);
		EVCASE(AV_PIX_FMT_X2BGR10BE);
		EVCASE(AV_PIX_FMT_P416BE);
		EVCASE(AV_PIX_FMT_P416LE);
#endif
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 2, 100)
		EVCASE(AV_PIX_FMT_VUYA);
		EVCASE(AV_PIX_FMT_VUYX);
#endif
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 33, 101)
		EVCASE(AV_PIX_FMT_RGBAF16BE);
		EVCASE(AV_PIX_FMT_RGBAF16LE);
#endif
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 37, 100)
		EVCASE(AV_PIX_FMT_P012LE);
		EVCASE(AV_PIX_FMT_P012BE);
		EVCASE(AV_PIX_FMT_Y212BE);
		EVCASE(AV_PIX_FMT_Y212LE);
		EVCASE(AV_PIX_FMT_XV30BE);
		EVCASE(AV_PIX_FMT_XV30LE);
		EVCASE(AV_PIX_FMT_XV36BE);
		EVCASE(AV_PIX_FMT_XV36LE);
		EVCASE(AV_PIX_FMT_RGBF32BE);
		EVCASE(AV_PIX_FMT_RGBF32LE);
		EVCASE(AV_PIX_FMT_RGBAF32BE);
		EVCASE(AV_PIX_FMT_RGBAF32LE);
#endif
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(59, 10, 100)
		EVCASE(AV_PIX_FMT_P212BE);
		EVCASE(AV_PIX_FMT_P212LE);
		EVCASE(AV_PIX_FMT_P412BE);
		EVCASE(AV_PIX_FMT_P412LE);
#endif
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(59, 14, 100)
		EVCASE(AV_PIX_FMT_GBRAP14BE);
		EVCASE(AV_PIX_FMT_GBRAP14LE);
#endif
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(59,16,100)
		EVCASE(AV_PIX_FMT_D3D12);
#endif
		EVCASE(AV_PIX_FMT_NB);
		default:
			return "AV_PIX_FMT_UNKNOWN";
	}
}
#endif

/* Ubuntu 14.04 ships without the functions provided by avutil,
 * so define error to string methods here. */
#if !defined(av_err2str)
static inline char* error_string(char* errbuf, size_t errbuf_size, int errnum)
{
	av_strerror(errnum, errbuf, errbuf_size);
	return errbuf;
}

#define av_err2str(errnum) error_string((char[64])WINPR_C_ARRAY_INIT, 64, errnum)
#endif

typedef struct
{
	const AVCodec* codecDecoder;
	AVCodecContext* codecDecoderContext;
	const AVCodec* codecEncoder;
	AVCodecContext* codecEncoderContext;
	AVCodecParserContext* codecParser;
	AVFrame* videoFrame;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 133, 100)
	AVPacket bufferpacket;
#endif
	AVPacket* packet;
#if defined(WITH_FFMPEG_HWACCEL)
	AVBufferRef* hwctx;
	struct SwsContext* swsctx;
	int hwFrameSupportsNativeFormat;
	AVFrame* hwVideoFrame;
	AVFrame* cpuVideoFrame;
	enum AVPixelFormat hw_pix_fmt;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 80, 100)
	AVBufferRef* hw_frames_ctx;
#endif

	enum AVHWDeviceType type;
	char* device;
	BOOL useHwDecoder;
	BOOL useHwEncoder;
#endif
} H264_CONTEXT_LIBAVCODEC;

static void libavcodec_destroy_encoder_context(H264_CONTEXT* WINPR_RESTRICT h264)
{
	if (!h264 || !h264->subsystem)
		return;

	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*)h264->pSystemData;

	if (sys->codecEncoderContext)
	{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 69, 100)
		avcodec_free_context(&sys->codecEncoderContext);
#else
		avcodec_close(sys->codecEncoderContext);
		av_free(sys->codecEncoderContext);
#endif
	}

	sys->codecEncoderContext = nullptr;
}

#if defined(WITH_FFMPEG_HWACCEL)
WINPR_ATTR_NODISCARD
static enum AVPixelFormat get_hwctx_image_format(H264_CONTEXT_LIBAVCODEC* sys, wLog* log)
{
	WINPR_ASSERT(sys);
	enum AVHWDeviceType type = sys->type;
	switch (type)
	{
		case AV_HWDEVICE_TYPE_VAAPI:
			return AV_PIX_FMT_VAAPI;
		case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
			return AV_PIX_FMT_VIDEOTOOLBOX;
		case AV_HWDEVICE_TYPE_VDPAU:
			return AV_PIX_FMT_VDPAU;
		case AV_HWDEVICE_TYPE_CUDA:
			return AV_PIX_FMT_CUDA;
		case AV_HWDEVICE_TYPE_DXVA2:
			return AV_PIX_FMT_DXVA2_VLD;
		case AV_HWDEVICE_TYPE_QSV:
			return AV_PIX_FMT_QSV;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(59, 16, 100)
		case AV_HWDEVICE_TYPE_D3D12VA:
			return AV_PIX_FMT_D3D12;
#endif
		case AV_HWDEVICE_TYPE_D3D11VA:
			return AV_PIX_FMT_D3D11VA_VLD;
		case AV_HWDEVICE_TYPE_DRM:
			return AV_PIX_FMT_DRM_PRIME;
		case AV_HWDEVICE_TYPE_OPENCL:
			return AV_PIX_FMT_OPENCL;
		case AV_HWDEVICE_TYPE_MEDIACODEC:
			return AV_PIX_FMT_MEDIACODEC;
		case AV_HWDEVICE_TYPE_VULKAN:
			return AV_PIX_FMT_VULKAN;
		default:
			WLog_Print(log, WLOG_INFO, "Missing pixel format mapping for %s",
			           av_hwdevice_get_type_name(type));
			return AV_PIX_FMT_NONE;
	}
}

WINPR_ATTR_NODISCARD
static const AVCodec* get_hwctx_codec(H264_CONTEXT_LIBAVCODEC* sys, wLog* log,
                                      enum AVHWDeviceType type)
{
	WINPR_ASSERT(sys);
	switch (type)
	{
		case AV_HWDEVICE_TYPE_VAAPI:
			return avcodec_find_encoder_by_name("h264_vaapi");
		case AV_HWDEVICE_TYPE_VULKAN:
			return avcodec_find_encoder_by_name("h264_vulkan");
		case AV_HWDEVICE_TYPE_D3D11VA:
			return avcodec_find_encoder_by_name("h264_d3d11va");
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(59, 16, 100)
		case AV_HWDEVICE_TYPE_D3D12VA:
			return avcodec_find_encoder_by_name("h264_d3d12va");
#endif
		case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
			return avcodec_find_encoder_by_name("h264_videotoolbox");
		case AV_HWDEVICE_TYPE_VDPAU:
			return avcodec_find_encoder_by_name("h264_vdpau");
		default:
			WLog_Print(log, WLOG_ERROR, "could not map %s to h264 encoder",
			           av_hwdevice_get_type_name(sys->type));
			return nullptr;
	}
}

WINPR_ATTR_NODISCARD
static const char* get_hwctx_device(H264_CONTEXT_LIBAVCODEC* sys)
{
	WINPR_ASSERT(sys);

	switch (sys->type)
	{
		case AV_HWDEVICE_TYPE_VAAPI:
			break;
		default:
			return nullptr;
	}

	return sys->device;
}
#endif

#ifdef WITH_FFMPEG_HWACCEL
static int set_hw_frames_ctx(H264_CONTEXT* WINPR_RESTRICT h264)
{
	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*)h264->pSystemData;
	AVBufferRef* hw_frames_ref = nullptr;
	AVHWFramesContext* frames_ctx = nullptr;
	int err = 0;

	if (!(hw_frames_ref = av_hwframe_ctx_alloc(sys->hwctx)))
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to create %s frame context",
		           av_hwdevice_get_type_name(sys->type));
		return -1;
	}
	frames_ctx = (AVHWFramesContext*)(hw_frames_ref->data);
	frames_ctx->format = get_hwctx_image_format(sys, h264->log);
	frames_ctx->sw_format = AV_PIX_FMT_NV12;
	frames_ctx->width = sys->codecEncoderContext->width;
	frames_ctx->height = sys->codecEncoderContext->height;
	frames_ctx->initial_pool_size = 20;
	if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0)
	{
		WLog_Print(h264->log, WLOG_ERROR,
		           "Failed to initialize %s frame context."
		           "Error code: %s",
		           av_hwdevice_get_type_name(sys->type), av_err2str(err));
		av_buffer_unref(&hw_frames_ref);
		return err;
	}
	sys->codecEncoderContext->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
	if (!sys->codecEncoderContext->hw_frames_ctx)
		err = AVERROR(ENOMEM);

	av_buffer_unref(&hw_frames_ref);
	return err;
}
#endif

static BOOL libavcodec_create_encoder_context(H264_CONTEXT* WINPR_RESTRICT h264)
{
	BOOL recreate = FALSE;
	H264_CONTEXT_LIBAVCODEC* sys = nullptr;

	if (!h264 || !h264->subsystem)
		return FALSE;

	if ((h264->width > INT_MAX) || (h264->height > INT_MAX))
		return FALSE;

	sys = (H264_CONTEXT_LIBAVCODEC*)h264->pSystemData;
	if (!sys || !sys->codecEncoder)
		return FALSE;

	recreate = !sys->codecEncoderContext;

	if (sys->codecEncoderContext)
	{
		if ((sys->codecEncoderContext->width != (int)h264->width) ||
		    (sys->codecEncoderContext->height != (int)h264->height))
			recreate = TRUE;
	}

	if (!recreate)
		return TRUE;

	libavcodec_destroy_encoder_context(h264);

	sys->codecEncoderContext = avcodec_alloc_context3(sys->codecEncoder);

	if (!sys->codecEncoderContext)
		goto EXCEPTION;

	switch (h264->RateControlMode)
	{
		case H264_RATECONTROL_VBR:
			sys->codecEncoderContext->bit_rate = h264->BitRate;
			break;

		case H264_RATECONTROL_CQP:
			if (av_opt_set_int(sys->codecEncoderContext, "qp", h264->QP, AV_OPT_SEARCH_CHILDREN) <
			    0)
			{
				WLog_Print(h264->log, WLOG_ERROR, "av_opt_set_int failed");
			}
			break;

		default:
			break;
	}

	sys->codecEncoderContext->width = (int)MIN(INT32_MAX, h264->width);
	sys->codecEncoderContext->height = (int)MIN(INT32_MAX, h264->height);
	sys->codecEncoderContext->delay = 0;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(56, 13, 100)
	sys->codecEncoderContext->framerate =
	    (AVRational){ WINPR_ASSERTING_INT_CAST(int, h264->FrameRate), 1 };
#endif
	sys->codecEncoderContext->time_base =
	    (AVRational){ 1, WINPR_ASSERTING_INT_CAST(int, h264->FrameRate) };
	av_opt_set(sys->codecEncoderContext, "tune", "zerolatency", AV_OPT_SEARCH_CHILDREN);

	sys->codecEncoderContext->flags |= AV_CODEC_FLAG_LOOP_FILTER;

#ifdef WITH_FFMPEG_HWACCEL
	if (sys->hwctx)
	{
		av_opt_set(sys->codecEncoderContext, "preset", "veryslow", AV_OPT_SEARCH_CHILDREN);

		sys->codecEncoderContext->pix_fmt = get_hwctx_image_format(sys, h264->log);
		/* set hw_frames_ctx for encoder's AVCodecContext */
		if (set_hw_frames_ctx(h264) < 0)
			goto EXCEPTION;
	}
	else
#endif
	{
		av_opt_set(sys->codecEncoderContext, "preset", "medium", AV_OPT_SEARCH_CHILDREN);
		sys->codecEncoderContext->pix_fmt = AV_PIX_FMT_YUV420P;
	}

	if (avcodec_open2(sys->codecEncoderContext, sys->codecEncoder, nullptr) < 0)
		goto EXCEPTION;

	return TRUE;
EXCEPTION:
	libavcodec_destroy_encoder_context(h264);
	return FALSE;
}

#if defined(WITH_FFMPEG_HWACCEL)
static int hw_supports_target_format(AVBufferRef* hwframectx, enum AVPixelFormat target)
{
	WINPR_ASSERT(hwframectx);

	enum AVPixelFormat* formats = nullptr;
	int rc = av_hwframe_transfer_get_formats(hwframectx, AV_HWFRAME_TRANSFER_DIRECTION_FROM,
	                                         &formats, 0);
	if (rc == 0)
	{
		enum AVPixelFormat* it = formats;
		while ((rc == 0) && (*it != AV_PIX_FMT_NONE))
		{
			enum AVPixelFormat cur = *it++;
			if (cur == target)
				rc = 1;
		}
	}

	av_free(formats);
	return rc;
}
#endif

static int libavcodec_decompress(H264_CONTEXT* WINPR_RESTRICT h264,
                                 const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcSize)
{
	union
	{
		const BYTE* cpv;
		BYTE* pv;
	} cnv;
	int rc = -1;
	int status = 0;
	int gotFrame = 0;
	AVPacket* packet = nullptr;

	WINPR_ASSERT(h264);
	WINPR_ASSERT(pSrcData || (SrcSize == 0));

	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*)h264->pSystemData;
	BYTE** pYUVData = h264->pYUVData;
	UINT32* iStride = h264->iStride;

	WINPR_ASSERT(sys);

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 133, 100)
	packet = &sys->bufferpacket;
	WINPR_ASSERT(packet);
	av_init_packet(packet);
#else
	packet = av_packet_alloc();
#endif
	if (!packet)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to allocate AVPacket");
		goto fail;
	}

	cnv.cpv = pSrcData;
	packet->data = cnv.pv;
	packet->size = (int)MIN(SrcSize, INT32_MAX);

	WINPR_ASSERT(sys->codecDecoderContext);
	/* avcodec_decode_video2 is deprecated with libavcodec 57.48.101 */
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	status = avcodec_send_packet(sys->codecDecoderContext, packet);

	if (status < 0)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to decode video frame (status=%d)", status);
		goto fail;
	}

	sys->videoFrame->format = AV_PIX_FMT_YUV420P;

#if defined(WITH_FFMPEG_HWACCEL)
	status = avcodec_receive_frame(sys->codecDecoderContext, (sys->useHwDecoder && sys->hwctx)
	                                                             ? sys->hwVideoFrame
	                                                             : sys->videoFrame);
#else
	status = avcodec_receive_frame(sys->codecDecoderContext, sys->videoFrame);
#endif
	if (status == AVERROR(EAGAIN))
	{
		rc = 0;
		goto fail;
	}

	if (status == 0)
		gotFrame = 1;
#else
#if defined(WITH_FFMPEG_HWACCEL)
	status =
	    avcodec_decode_video2(sys->codecDecoderContext,
	                          sys->hwctx ? sys->hwVideoFrame : sys->videoFrame, &gotFrame, packet);
#else
	status = avcodec_decode_video2(sys->codecDecoderContext, sys->videoFrame, &gotFrame, packet);
#endif
#endif
	if (status < 0)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to decode video frame (status=%d)", status);
		goto fail;
	}

#if defined(WITH_FFMPEG_HWACCEL)

	if (sys->hwctx)
	{
		AVFrame* target = sys->videoFrame;
		if (sys->hwFrameSupportsNativeFormat < 0)
		{
			const int res = hw_supports_target_format(sys->codecDecoderContext->hw_frames_ctx,
			                                          sys->videoFrame->format);
			if (res < 0)
				status = res;
			else
			{
				sys->hwFrameSupportsNativeFormat = res;
				const char* msg = "hardware supported copy";
				if (res == 0)
					msg = "swscale format conversion";
				WLog_Print(h264->log, WLOG_DEBUG, "formats: { src:%s, dst:%s } using %s",
				           av_format_str(sys->hwVideoFrame->format),
				           av_format_str(sys->videoFrame->format), msg);
			}
		}

		if (sys->hwFrameSupportsNativeFormat == 0)
			target = sys->cpuVideoFrame;

		if (sys->hwFrameSupportsNativeFormat >= 0)
		{
			if (sys->hwVideoFrame->format == sys->hw_pix_fmt)
			{
				target->width = sys->hwVideoFrame->width;
				target->height = sys->hwVideoFrame->height;
				status = av_hwframe_transfer_data(target, sys->hwVideoFrame, 0);
			}
			else
				status = av_frame_copy(target, sys->hwVideoFrame);
		}

		gotFrame = (status == 0);

		if (status < 0)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Failed to transfer video frame (status=%d) (%s)",
			           status, av_err2str(status));
			goto fail;
		}

		if (gotFrame && (target != sys->videoFrame))
		{
			if (!sys->swsctx)
			{
				sys->swsctx = sws_getContext(target->width, target->height, target->format,
				                             target->width, target->height, sys->videoFrame->format,
				                             SWS_BILINEAR, nullptr, nullptr, nullptr);
				if (!sys->swsctx)
					goto fail;
			}

#if LIBSWSCALE_VERSION_INT >= AV_VERSION_INT(6, 4, 100)
			status = sws_scale_frame(sys->swsctx, sys->videoFrame, target);
#else
#error "Need SWScale >= 6.4.100"
#endif

			if (status < 0)
			{
				WLog_Print(h264->log, WLOG_ERROR, "Failed to convert video frame (status=%d) (%s)",
				           status, av_err2str(status));
				goto fail;
			}
		}
	}

#endif

	if (gotFrame)
	{
		WINPR_ASSERT(sys->videoFrame);

		pYUVData[0] = sys->videoFrame->data[0];
		pYUVData[1] = sys->videoFrame->data[1];
		pYUVData[2] = sys->videoFrame->data[2];
		iStride[0] = (UINT32)MAX(0, sys->videoFrame->linesize[0]);
		iStride[1] = (UINT32)MAX(0, sys->videoFrame->linesize[1]);
		iStride[2] = (UINT32)MAX(0, sys->videoFrame->linesize[2]);

		h264->YUVWidth = WINPR_ASSERTING_INT_CAST(UINT32, sys->videoFrame->width);
		h264->YUVHeight = WINPR_ASSERTING_INT_CAST(UINT32, sys->videoFrame->height);
		rc = 1;
	}
	else
		rc = -2;

fail:
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 133, 100)
	av_packet_unref(packet);
#else
	av_packet_free(&packet);
#endif

	return rc;
}

static int libavcodec_compress(H264_CONTEXT* WINPR_RESTRICT h264,
                               const BYTE** WINPR_RESTRICT pSrcYuv,
                               const UINT32* WINPR_RESTRICT pStride,
                               BYTE** WINPR_RESTRICT ppDstData, UINT32* WINPR_RESTRICT pDstSize)
{
	union
	{
		const BYTE* cpv;
		uint8_t* pv;
	} cnv;
	int rc = -1;
	int status = 0;
	int gotFrame = 0;

	WINPR_ASSERT(h264);

	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	if (!libavcodec_create_encoder_context(h264))
		return -1;

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 133, 100)
	sys->packet = &sys->bufferpacket;
	av_packet_unref(sys->packet);
	av_init_packet(sys->packet);
#else
	av_packet_free(&sys->packet);
	sys->packet = av_packet_alloc();
#endif
	if (!sys->packet)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to allocate AVPacket");
		goto fail;
	}

	WINPR_ASSERT(sys->packet);
	sys->packet->data = nullptr;
	sys->packet->size = 0;

	WINPR_ASSERT(sys->videoFrame);
	WINPR_ASSERT(sys->codecEncoderContext);
	sys->videoFrame->format = AV_PIX_FMT_YUV420P;
	sys->videoFrame->width = sys->codecEncoderContext->width;
	sys->videoFrame->height = sys->codecEncoderContext->height;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 48, 100)
	sys->videoFrame->colorspace = AVCOL_SPC_BT709;
#endif
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 92, 100)
	sys->videoFrame->chroma_location = AVCHROMA_LOC_LEFT;
#endif
	cnv.cpv = pSrcYuv[0];
	sys->videoFrame->data[0] = cnv.pv;

	cnv.cpv = pSrcYuv[1];
	sys->videoFrame->data[1] = cnv.pv;

	cnv.cpv = pSrcYuv[2];
	sys->videoFrame->data[2] = cnv.pv;

	sys->videoFrame->linesize[0] = (int)pStride[0];
	sys->videoFrame->linesize[1] = (int)pStride[1];
	sys->videoFrame->linesize[2] = (int)pStride[2];
	sys->videoFrame->pts++;

#ifdef WITH_FFMPEG_HWACCEL
	if (sys->useHwEncoder && sys->hwctx)
	{
		av_frame_unref(sys->hwVideoFrame);
		if ((status = av_hwframe_get_buffer(sys->codecEncoderContext->hw_frames_ctx,
		                                    sys->hwVideoFrame, 0)) < 0 ||
		    !sys->hwVideoFrame->hw_frames_ctx)
		{
			WLog_Print(h264->log, WLOG_ERROR, "av_hwframe_get_buffer failed (%s [%d])",
			           av_err2str(status), status);
			goto fail;
		}
		sys->videoFrame->format = AV_PIX_FMT_NV12;
		if ((status = av_hwframe_transfer_data(sys->hwVideoFrame, sys->videoFrame, 0)) < 0)
		{
			WLog_Print(h264->log, WLOG_ERROR, "av_hwframe_transfer_data failed (%s [%d])",
			           av_err2str(status), status);
			goto fail;
		}
	}
#endif

	/* avcodec_encode_video2 is deprecated with libavcodec 57.48.101 */
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
#ifdef WITH_FFMPEG_HWACCEL
	status = avcodec_send_frame(sys->codecEncoderContext,
	                            sys->hwctx ? sys->hwVideoFrame : sys->videoFrame);
#else
	status = avcodec_send_frame(sys->codecEncoderContext, sys->videoFrame);
#endif

	if (status < 0)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to encode video frame (%s [%d])",
		           av_err2str(status), status);
		goto fail;
	}

	status = avcodec_receive_packet(sys->codecEncoderContext, sys->packet);

	if (status < 0)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to encode video frame (%s [%d])",
		           av_err2str(status), status);
		goto fail;
	}

	gotFrame = (status == 0);
#elif LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(54, 59, 100)

	do
	{
		status = avcodec_encode_video2(sys->codecEncoderContext, sys->packet, sys->videoFrame,
		                               &gotFrame);
	} while ((status >= 0) && (gotFrame == 0));

#else
	sys->packet->size =
	    avpicture_get_size(sys->codecDecoderContext->pix_fmt, sys->codecDecoderContext->width,
	                       sys->codecDecoderContext->height);
	sys->packet->data = av_malloc(sys->packet->size);

	if (!sys->packet->data)
		status = -1;
	else
	{
		status = avcodec_encode_video(sys->codecDecoderContext, sys->packet->data,
		                              sys->packet->size, sys->videoFrame);
	}

#endif

	if (status < 0)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to encode video frame (%s [%d])",
		           av_err2str(status), status);
		goto fail;
	}

	WINPR_ASSERT(sys->packet);
	*ppDstData = sys->packet->data;
	*pDstSize = (UINT32)MAX(0, sys->packet->size);

	if (!gotFrame)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Did not get frame! (%s [%d])", av_err2str(status),
		           status);
		rc = -2;
	}
	else
		rc = 1;
fail:
	return rc;
}

static void libavcodec_uninit(H264_CONTEXT* h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*)h264->pSystemData;

	if (!sys)
		return;

	if (sys->packet)
	{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 133, 100)
		av_packet_unref(sys->packet);
#else
		av_packet_free(&sys->packet);
#endif
	}

	if (sys->videoFrame)
	{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 18, 102)
		av_frame_free(&sys->videoFrame);
#else
		av_free(sys->videoFrame);
#endif
	}

#if defined(WITH_FFMPEG_HWACCEL)
	if (sys->hwVideoFrame)
	{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 18, 102)
		av_frame_free(&sys->hwVideoFrame);
#else
		av_free(sys->hwVideoFrame);
		sys->hwVideoFrame = nullptr;
#endif
	}

	if (sys->cpuVideoFrame)
	{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 18, 102)
		av_frame_free(&sys->cpuVideoFrame);
#else
		av_free(sys->cpuVideoFrame);
#endif
	}

	if (sys->hwctx)
		av_buffer_unref(&sys->hwctx);
	sws_freeContext(sys->swsctx);

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 80, 100)

	if (sys->hw_frames_ctx)
		av_buffer_unref(&sys->hw_frames_ctx);

#endif

#endif

	if (sys->codecParser)
		av_parser_close(sys->codecParser);

	if (sys->codecDecoderContext)
	{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 69, 100)
		avcodec_free_context(&sys->codecDecoderContext);
#else
		avcodec_close(sys->codecDecoderContext);
		av_free(sys->codecDecoderContext);
#endif
	}

	libavcodec_destroy_encoder_context(h264);
#if defined(WITH_FFMPEG_HWACCEL)
	free(sys->device);
#endif
	free(sys);
	h264->pSystemData = nullptr;
}

#if defined(WITH_FFMPEG_HWACCEL)
WINPR_ATTR_NODISCARD
static const char* vaapi_vendor(H264_CONTEXT_LIBAVCODEC* sys)
{
	WINPR_ASSERT(sys);

	const char* vendor = "UNKNOWN";
#if 0
	if (!hwctx)
		return vendor;

	const AVHWDeviceContext* devctx = (const AVHWDeviceContext*)hwctx->data;
	if (!devctx)
		return vendor;

	const AVVAAPIDeviceContext* ctx = devctx->hwctx;
	if (!ctx)
		return vendor;

	const char* vavendor = vaQueryVendorString(ctx->display);
	if (vavendor)
		return vavendor;
#endif
	return vendor;
}
#endif
#if defined(WITH_FFMPEG_HWACCEL)
WINPR_ATTR_NODISCARD
static enum AVPixelFormat libavcodec_get_format(struct AVCodecContext* ctx,
                                                const enum AVPixelFormat* fmts)
{
	WINPR_ASSERT(ctx);

	H264_CONTEXT* h264 = (H264_CONTEXT*)ctx->opaque;
	WINPR_ASSERT(h264);

	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	for (const enum AVPixelFormat* p = fmts; *p != AV_PIX_FMT_NONE; p++)
	{
		if (*p == sys->hw_pix_fmt)
		{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 80, 100)
			if (sys->hw_frames_ctx)
				av_buffer_unref(&sys->hw_frames_ctx);

			sys->hw_frames_ctx = av_hwframe_ctx_alloc(sys->hwctx);

			if (!sys->hw_frames_ctx)
			{
				return AV_PIX_FMT_NONE;
			}

			sys->codecDecoderContext->pix_fmt = *p;
			AVHWFramesContext* frames = (AVHWFramesContext*)sys->hw_frames_ctx->data;
			frames->format = *p;
			frames->height = sys->codecDecoderContext->coded_height;
			frames->width = sys->codecDecoderContext->coded_width;
#ifdef WITH_VIDEOTOOLBOX
			frames->sw_format = AV_PIX_FMT_YUV420P;
#else
			frames->sw_format =
			    (sys->codecDecoderContext->sw_pix_fmt == AV_PIX_FMT_YUV420P10 ? AV_PIX_FMT_P010
			                                                                  : AV_PIX_FMT_NV12);
#endif
			frames->initial_pool_size = 20;

			if (sys->codecDecoderContext->active_thread_type & FF_THREAD_FRAME)
				frames->initial_pool_size += sys->codecDecoderContext->thread_count;

			int err = av_hwframe_ctx_init(sys->hw_frames_ctx);

			if (err < 0)
			{
				WLog_Print(h264->log, WLOG_ERROR, "Could not init hwframes context: %s",
				           av_err2str(err));
				return AV_PIX_FMT_NONE;
			}

			sys->codecDecoderContext->hw_frames_ctx = av_buffer_ref(sys->hw_frames_ctx);
#endif
			return *p;
		}
	}

	return AV_PIX_FMT_NONE;
}

WINPR_ATTR_MALLOC(WINPR_JSON_Delete, 1)
static WINPR_JSON* load(BOOL system)
{
	return freerdp_GetJSONConfigFile(system, "ffmpeg.json");
}

static BOOL read_json_bool(WINPR_JSON* json, const char* what, BOOL* pValue)
{
	WINPR_ASSERT(json);
	WINPR_ASSERT(what);
	WINPR_ASSERT(pValue);

	WINPR_JSON* val = WINPR_JSON_GetObjectItemCaseSensitive(json, what);
	if (!val)
		return FALSE;
	if (!WINPR_JSON_IsBool(val))
		return FALSE;
	*pValue = WINPR_JSON_IsTrue(val);
	return TRUE;
}

static BOOL read_json_const_string(WINPR_JSON* json, const char* what, const char** pValue)
{
	WINPR_ASSERT(json);
	WINPR_ASSERT(what);
	WINPR_ASSERT(pValue);

	WINPR_JSON* val = WINPR_JSON_GetObjectItemCaseSensitive(json, what);
	if (!val)
		return FALSE;
	if (!WINPR_JSON_IsString(val))
		return FALSE;
	*pValue = WINPR_JSON_GetStringValue(val);
	return *pValue != nullptr;
}

static BOOL read_json_string(WINPR_JSON* json, const char* what, char** pValue)
{
	WINPR_ASSERT(json);
	WINPR_ASSERT(what);
	WINPR_ASSERT(pValue);

	WINPR_JSON* val = WINPR_JSON_GetObjectItemCaseSensitive(json, what);
	if (!val)
		return FALSE;
	if (!WINPR_JSON_IsString(val))
		return FALSE;
	const char* str = nullptr;
	const BOOL rc = read_json_const_string(json, what, &str);
	if (!rc)
		return FALSE;
	if (!str)
		return FALSE;
	free(*pValue);
	*pValue = _strdup(str);
	return *pValue != nullptr;
}

static BOOL load_config_json(BOOL system, H264_CONTEXT_LIBAVCODEC* sys, BOOL* pAllowUserConfig,
                             BOOL* pAllowEnvOverride)
{
	WINPR_ASSERT(sys);
	WINPR_ASSERT(pAllowUserConfig);
	WINPR_ASSERT(pAllowEnvOverride);

	WINPR_JSON* syscfg = load(system);
	if (syscfg)
	{
		read_json_bool(syscfg, "allowUserConfig", pAllowUserConfig);
		read_json_bool(syscfg, "allowEnvOverrides", pAllowEnvOverride);

		read_json_string(syscfg, "hwDeviceName", &sys->device);

		const char* str = nullptr;
		if (read_json_const_string(syscfg, "hwDeviceType", &str))
			sys->type = av_hwdevice_find_type_by_name(str);
		read_json_bool(syscfg, "useHwDecoder", &sys->useHwDecoder);
		read_json_bool(syscfg, "useHwEncoder", &sys->useHwEncoder);
		WINPR_JSON_Delete(syscfg);
	}
	return TRUE;
}
#endif

WINPR_ATTR_NODISCARD
static BOOL load_config(H264_CONTEXT_LIBAVCODEC* sys, BOOL enabled, wLog* log)
{
	WINPR_ASSERT(sys);

#if defined(WITH_FFMPEG_HWACCEL)
	sys->useHwDecoder = TRUE;
	sys->useHwEncoder = TRUE;
#if defined(__APPLE__)
	sys->type = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
#elif defined(__ANDROID__)
	sys->type = AV_HWDEVICE_TYPE_MEDIACODEC;
#elif defined(_WIN32)
	sys->type = AV_HWDEVICE_TYPE_DXVA2;
#elif defined(__linux__)
	sys->type = AV_HWDEVICE_TYPE_VAAPI;
	sys->device = _strdup("/dev/dri/renderD128");
	if (!sys->device)
		return FALSE;
#else
	sys->type = AV_HWDEVICE_TYPE_NONE;
#endif

	BOOL allowUserConfig = TRUE;
	BOOL allowEnvOverrides = TRUE;

	load_config_json(TRUE, sys, &allowUserConfig, &allowEnvOverrides);
	if (allowUserConfig)
		load_config_json(FALSE, sys, &allowUserConfig, &allowEnvOverrides);

	if (allowEnvOverrides)
	{
		// NOLINTNEXTLINE(concurrency-mt-unsafe)
		const char* env = getenv("FREERDP_HWCTX_DEVICE");
		if (!env)
			env = getenv("FREERDP_VAAPI_DEVICE");

		if (env)
		{
			free(sys->device);
			sys->device = _strdup(env);
			if (!sys->device)
			{
				WLog_Print(log, WLOG_ERROR,
				           "Failed to copy value of FREERDP_HWCTX_TYPE=%s, aborting", env);
				return FALSE;
			}
		}

		env = getenv("FREERDP_HWCTX_TYPE");
		if (env)
		{
			sys->type = av_hwdevice_find_type_by_name(env);
			if (sys->type == AV_HWDEVICE_TYPE_NONE)
			{
				WLog_Print(log, WLOG_ERROR, "Unsupported value for FREERDP_HWCTX_TYPE=%s, aborting",
				           env);
				return FALSE;
			}
		}
	}

	// Fallback in case of an invalid type
	if (!enabled || (sys->type == AV_HWDEVICE_TYPE_NONE))
	{
		sys->useHwDecoder = FALSE;
		sys->useHwEncoder = FALSE;
	}
#else
	WINPR_UNUSED(enabled);
	WINPR_UNUSED(log);
#endif
	return TRUE;
}

#if defined(WITH_FFMPEG_HWACCEL)
WINPR_ATTR_NODISCARD
static const char* bool_to_string(BOOL val)
{
	if (val)
		return "true";
	return "false";
}
#endif

static BOOL libavcodec_init(H264_CONTEXT* h264)
{
	WINPR_ASSERT(h264);
	H264_CONTEXT_LIBAVCODEC* sys =
	    (H264_CONTEXT_LIBAVCODEC*)calloc(1, sizeof(H264_CONTEXT_LIBAVCODEC));

	if (!sys)
		goto EXCEPTION;

	if (!load_config(sys, h264->hwAccel > 0, h264->log))
		goto EXCEPTION;

	h264->pSystemData = (void*)sys;

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 10, 100)
	avcodec_register_all();
#endif

	if (!h264->Compressor)
	{
		sys->codecDecoder = avcodec_find_decoder(AV_CODEC_ID_H264);

		if (!sys->codecDecoder)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Failed to find libav H.264 codec");
			goto EXCEPTION;
		}

		sys->codecDecoderContext = avcodec_alloc_context3(sys->codecDecoder);

		if (!sys->codecDecoderContext)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Failed to allocate libav codec context");
			goto EXCEPTION;
		}

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 18, 100)
		if (sys->codecDecoder->capabilities & AV_CODEC_CAP_TRUNCATED)
		{
			sys->codecDecoderContext->flags |= AV_CODEC_FLAG_TRUNCATED;
		}
#endif

#ifdef WITH_FFMPEG_HWACCEL
		if (sys->useHwDecoder && (sys->type != AV_HWDEVICE_TYPE_NONE))
			WLog_Print(h264->log, WLOG_DEBUG,
			           "Trying to initialize hardware backed decoder of type %s",
			           av_hwdevice_get_type_name(sys->type));
		else
			WLog_Print(h264->log, WLOG_DEBUG,
			           "Skipping hardware backed decoder (useHwDecoder=%s, type=%s)",
			           bool_to_string(sys->useHwDecoder), av_hwdevice_get_type_name(sys->type));

		if (sys->useHwDecoder && (sys->type != AV_HWDEVICE_TYPE_NONE))
		{
			sys->hwFrameSupportsNativeFormat = -1;
			enum AVHWDeviceType type = sys->type;
			if (!sys->hwctx)
			{
				int ret =
				    av_hwdevice_ctx_create(&sys->hwctx, type, get_hwctx_device(sys), nullptr, 0);

				if (ret < 0)
				{
					WLog_Print(h264->log, WLOG_ERROR,
					           "Could not initialize hardware decoder for %s, falling back to "
					           "software: %s",
					           get_hwctx_device(sys), av_err2str(ret));
					sys->hwctx = nullptr;
					goto fail_hwdevice_create;
				}
			}
			WLog_Print(h264->log, WLOG_INFO, "Using %s [%s|%s] for accelerated H264 decoding",
			           av_hwdevice_get_type_name(type), get_hwctx_device(sys), vaapi_vendor(sys));

			sys->codecDecoderContext->get_format = libavcodec_get_format;
			sys->hw_pix_fmt = get_hwctx_image_format(sys, h264->log);

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 80, 100)
		sys->codecDecoderContext->hw_device_ctx = av_buffer_ref(sys->hwctx);
#endif
		sys->codecDecoderContext->opaque = (void*)h264;

	fail_hwdevice_create:
	}
#endif

		if (avcodec_open2(sys->codecDecoderContext, sys->codecDecoder, nullptr) < 0)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Failed to open libav codec");
			goto EXCEPTION;
		}

		sys->codecParser = av_parser_init(AV_CODEC_ID_H264);

		if (!sys->codecParser)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Failed to initialize libav parser");
			goto EXCEPTION;
		}
	}
	else
	{
#ifdef WITH_FFMPEG_HWACCEL
		if (sys->useHwEncoder && (sys->type != AV_HWDEVICE_TYPE_NONE))
			WLog_Print(h264->log, WLOG_DEBUG,
			           "Trying to initialize hardware backed encoder of type %s",
			           av_hwdevice_get_type_name(sys->type));
		else
			WLog_Print(h264->log, WLOG_DEBUG,
			           "Skipping hardware backed encoder (useHwEncoder=%s, type=%s)",
			           bool_to_string(sys->useHwEncoder), av_hwdevice_get_type_name(sys->type));

		if (sys->useHwEncoder) /* user requested hw accel */
		{
			enum AVHWDeviceType type = sys->type;

			sys->codecEncoder = get_hwctx_codec(sys, h264->log, type);
			if (!sys->codecEncoder)
			{
				WLog_Print(h264->log, WLOG_ERROR, "H264 hardware encoder not found");
			}
			else if (av_hwdevice_ctx_create(&sys->hwctx, type, get_hwctx_device(sys), nullptr, 0) <
			         0)
			{
				WLog_Print(h264->log, WLOG_ERROR, "av_hwdevice_ctx_create(%s) failed",
				           get_hwctx_device(sys));
				sys->codecEncoder = nullptr;
				sys->hwctx = nullptr;
			}
			else
			{
				WLog_Print(h264->log, WLOG_INFO, "Using %s[%s|%s] for accelerated H264 encoding",
				           av_hwdevice_get_type_name(sys->type), get_hwctx_device(sys),
				           vaapi_vendor(sys));
			}
		}
#endif
		if (!sys->codecEncoder)
		{
			sys->codecEncoder = avcodec_find_encoder(AV_CODEC_ID_H264);
			h264->hwAccel = FALSE; /* not supported */
		}

		if (!sys->codecEncoder)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Failed to initialize H264 encoder");
			goto EXCEPTION;
		}
	}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 18, 102)
	sys->videoFrame = av_frame_alloc();
#if defined(WITH_FFMPEG_HWACCEL)
	sys->hwVideoFrame = av_frame_alloc();
	sys->cpuVideoFrame = av_frame_alloc();
#endif
#else
	sys->videoFrame = avcodec_alloc_frame();
#endif

	if (!sys->videoFrame)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to allocate libav frame");
		goto EXCEPTION;
	}

#if defined(WITH_FFMPEG_HWACCEL)
	if (!sys->hwVideoFrame || !sys->cpuVideoFrame)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to allocate libav hw frame");
		goto EXCEPTION;
	}

#endif
	sys->videoFrame->pts = 0;
	return TRUE;
EXCEPTION:
	libavcodec_uninit(h264);
	return FALSE;
}

const H264_CONTEXT_SUBSYSTEM g_Subsystem_libavcodec = { "libavcodec", libavcodec_init,
	                                                    libavcodec_uninit, libavcodec_decompress,
	                                                    libavcodec_compress };
