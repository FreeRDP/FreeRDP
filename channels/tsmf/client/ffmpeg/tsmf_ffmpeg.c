/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - FFmpeg Decoder
 *
 * Copyright 2010-2011 Vic Lee
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/utils/event.h>
#include <freerdp/client/tsmf.h>

#include <libavcodec/avcodec.h>

#include <pthread.h>

#include "tsmf_constants.h"
#include "tsmf_decoder.h"

/* Compatibility with older FFmpeg */
#if LIBAVUTIL_VERSION_MAJOR < 50
#define AVMEDIA_TYPE_VIDEO 0
#define AVMEDIA_TYPE_AUDIO 1
#endif

typedef struct _TSMFFFmpegDecoder
{
	ITSMFDecoder iface;

	int media_type;
	enum CodecID codec_id;
	AVCodecContext* codec_context;
	AVCodec* codec;
	AVFrame* frame;
	int prepared;

	BYTE* decoded_data;
	UINT32 decoded_size;
	UINT32 decoded_size_max;
} TSMFFFmpegDecoder;

static BOOL tsmf_ffmpeg_init_context(ITSMFDecoder* decoder)
{
	TSMFFFmpegDecoder* mdecoder = (TSMFFFmpegDecoder*) decoder;

	mdecoder->codec_context = avcodec_alloc_context3(NULL);

	if (!mdecoder->codec_context)
	{
		DEBUG_WARN("avcodec_alloc_context failed.");
		return FALSE;
	}

	return TRUE;
}

static BOOL tsmf_ffmpeg_init_video_stream(ITSMFDecoder* decoder, const TS_AM_MEDIA_TYPE* media_type)
{
	TSMFFFmpegDecoder* mdecoder = (TSMFFFmpegDecoder*) decoder;

	mdecoder->codec_context->width = media_type->Width;
	mdecoder->codec_context->height = media_type->Height;
	mdecoder->codec_context->bit_rate = media_type->BitRate;
	mdecoder->codec_context->time_base.den = media_type->SamplesPerSecond.Numerator;
	mdecoder->codec_context->time_base.num = media_type->SamplesPerSecond.Denominator;

	mdecoder->frame = avcodec_alloc_frame();

	return TRUE;
}

static BOOL tsmf_ffmpeg_init_audio_stream(ITSMFDecoder* decoder, const TS_AM_MEDIA_TYPE* media_type)
{
	TSMFFFmpegDecoder* mdecoder = (TSMFFFmpegDecoder*) decoder;

	mdecoder->codec_context->sample_rate = media_type->SamplesPerSecond.Numerator;
	mdecoder->codec_context->bit_rate = media_type->BitRate;
	mdecoder->codec_context->channels = media_type->Channels;
	mdecoder->codec_context->block_align = media_type->BlockAlign;

#ifdef AV_CPU_FLAG_SSE2
	mdecoder->codec_context->dsp_mask = AV_CPU_FLAG_SSE2 | AV_CPU_FLAG_MMX2;
#else
#if LIBAVCODEC_VERSION_MAJOR < 53
	mdecoder->codec_context->dsp_mask = FF_MM_SSE2 | FF_MM_MMXEXT;
#else
	mdecoder->codec_context->dsp_mask = FF_MM_SSE2 | FF_MM_MMX2;
#endif
#endif

	return TRUE;
}

static BOOL tsmf_ffmpeg_init_stream(ITSMFDecoder* decoder, const TS_AM_MEDIA_TYPE* media_type)
{
	BYTE* p;
	UINT32 size;
	const BYTE* s;
	TSMFFFmpegDecoder* mdecoder = (TSMFFFmpegDecoder*) decoder;

	mdecoder->codec = avcodec_find_decoder(mdecoder->codec_id);

	if (!mdecoder->codec)
	{
		DEBUG_WARN("avcodec_find_decoder failed.");
		return FALSE;
	}

	mdecoder->codec_context->codec_id = mdecoder->codec_id;
	mdecoder->codec_context->codec_type = mdecoder->media_type;

	if (mdecoder->media_type == AVMEDIA_TYPE_VIDEO)
	{
		if (!tsmf_ffmpeg_init_video_stream(decoder, media_type))
			return FALSE;
	}
	else if (mdecoder->media_type == AVMEDIA_TYPE_AUDIO)
	{
		if (!tsmf_ffmpeg_init_audio_stream(decoder, media_type))
			return FALSE;
	}

	if (media_type->ExtraData)
	{
		if (media_type->SubType == TSMF_SUB_TYPE_AVC1 &&
			media_type->FormatType == TSMF_FORMAT_TYPE_MPEG2VIDEOINFO)
		{
			/* The extradata format that FFmpeg uses is following CodecPrivate in Matroska.
			   See http://haali.su/mkv/codecs.pdf */
			mdecoder->codec_context->extradata_size = media_type->ExtraDataSize + 8;
			mdecoder->codec_context->extradata = malloc(mdecoder->codec_context->extradata_size);
			ZeroMemory(mdecoder->codec_context->extradata, mdecoder->codec_context->extradata_size);
			p = mdecoder->codec_context->extradata;
			*p++ = 1; /* Reserved? */
			*p++ = media_type->ExtraData[8]; /* Profile */
			*p++ = 0; /* Profile */
			*p++ = media_type->ExtraData[12]; /* Level */
			*p++ = 0xff; /* Flag? */
			*p++ = 0xe0 | 0x01; /* Reserved | #sps */
			s = media_type->ExtraData + 20;
			size = ((UINT32)(*s)) * 256 + ((UINT32)(*(s + 1)));
			memcpy(p, s, size + 2);
			s += size + 2;
			p += size + 2;
			*p++ = 1; /* #pps */
			size = ((UINT32)(*s)) * 256 + ((UINT32)(*(s + 1)));
			memcpy(p, s, size + 2);
		}
		else
		{
			/* Add a padding to avoid invalid memory read in some codec */
			mdecoder->codec_context->extradata_size = media_type->ExtraDataSize + 8;
			mdecoder->codec_context->extradata = malloc(mdecoder->codec_context->extradata_size);
			ZeroMemory(mdecoder->codec_context->extradata, mdecoder->codec_context->extradata_size);
			memcpy(mdecoder->codec_context->extradata, media_type->ExtraData, media_type->ExtraDataSize);
			memset(mdecoder->codec_context->extradata + media_type->ExtraDataSize, 0, 8);
		}
	}

	if (mdecoder->codec->capabilities & CODEC_CAP_TRUNCATED)
		mdecoder->codec_context->flags |= CODEC_FLAG_TRUNCATED;

	return TRUE;
}

static BOOL tsmf_ffmpeg_prepare(ITSMFDecoder* decoder)
{
	TSMFFFmpegDecoder* mdecoder = (TSMFFFmpegDecoder*) decoder;

	if (avcodec_open2(mdecoder->codec_context, mdecoder->codec, NULL) < 0)
	{
		DEBUG_WARN("avcodec_open2 failed.");
		return FALSE;
	}

	mdecoder->prepared = 1;

	return TRUE;
}

static BOOL tsmf_ffmpeg_set_format(ITSMFDecoder* decoder, TS_AM_MEDIA_TYPE* media_type)
{
	TSMFFFmpegDecoder* mdecoder = (TSMFFFmpegDecoder*) decoder;

	switch (media_type->MajorType)
	{
		case TSMF_MAJOR_TYPE_VIDEO:
			mdecoder->media_type = AVMEDIA_TYPE_VIDEO;
			break;
		case TSMF_MAJOR_TYPE_AUDIO:
			mdecoder->media_type = AVMEDIA_TYPE_AUDIO;
			break;
		default:
			return FALSE;
	}
	switch (media_type->SubType)
	{
		case TSMF_SUB_TYPE_WVC1:
			mdecoder->codec_id = CODEC_ID_VC1;
			break;
		case TSMF_SUB_TYPE_WMA2:
			mdecoder->codec_id = CODEC_ID_WMAV2;
			break;
		case TSMF_SUB_TYPE_WMA9:
			mdecoder->codec_id = CODEC_ID_WMAPRO;
			break;
		case TSMF_SUB_TYPE_MP3:
			mdecoder->codec_id = CODEC_ID_MP3;
			break;
		case TSMF_SUB_TYPE_MP2A:
			mdecoder->codec_id = CODEC_ID_MP2;
			break;
		case TSMF_SUB_TYPE_MP2V:
			mdecoder->codec_id = CODEC_ID_MPEG2VIDEO;
			break;
		case TSMF_SUB_TYPE_WMV3:
			mdecoder->codec_id = CODEC_ID_WMV3;
			break;
		case TSMF_SUB_TYPE_AAC:
			mdecoder->codec_id = CODEC_ID_AAC;
			/* For AAC the pFormat is a HEAACWAVEINFO struct, and the codec data
			   is at the end of it. See
			   http://msdn.microsoft.com/en-us/library/dd757806.aspx */
			if (media_type->ExtraData)
			{
				media_type->ExtraData += 12;
				media_type->ExtraDataSize -= 12;
			}
			break;
		case TSMF_SUB_TYPE_H264:
		case TSMF_SUB_TYPE_AVC1:
			mdecoder->codec_id = CODEC_ID_H264;
			break;
		case TSMF_SUB_TYPE_AC3:
			mdecoder->codec_id = CODEC_ID_AC3;
			break;
		default:
			return FALSE;
	}

	if (!tsmf_ffmpeg_init_context(decoder))
		return FALSE;
	if (!tsmf_ffmpeg_init_stream(decoder, media_type))
		return FALSE;
	if (!tsmf_ffmpeg_prepare(decoder))
		return FALSE;

	return TRUE;
}

static BOOL tsmf_ffmpeg_decode_video(ITSMFDecoder* decoder, const BYTE* data, UINT32 data_size, UINT32 extensions)
{
	TSMFFFmpegDecoder* mdecoder = (TSMFFFmpegDecoder*) decoder;
	int decoded;
	int len;
	AVFrame* frame;
	BOOL ret = TRUE;

#if LIBAVCODEC_VERSION_MAJOR < 52 || (LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR <= 20)
	len = avcodec_decode_video(mdecoder->codec_context, mdecoder->frame, &decoded, data, data_size);
#else
	{
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = (BYTE*) data;
		pkt.size = data_size;
		if (extensions & TSMM_SAMPLE_EXT_CLEANPOINT)
			pkt.flags |= AV_PKT_FLAG_KEY;
		len = avcodec_decode_video2(mdecoder->codec_context, mdecoder->frame, &decoded, &pkt);
	}
#endif

	if (len < 0)
	{
		DEBUG_WARN("data_size %d, avcodec_decode_video failed (%d)", data_size, len);
		ret = FALSE;
	}
	else if (!decoded)
	{
		DEBUG_WARN("data_size %d, no frame is decoded.", data_size);
		ret = FALSE;
	}
	else
	{
		DEBUG_DVC("linesize[0] %d linesize[1] %d linesize[2] %d linesize[3] %d "
			"pix_fmt %d width %d height %d",
			mdecoder->frame->linesize[0], mdecoder->frame->linesize[1],
			mdecoder->frame->linesize[2], mdecoder->frame->linesize[3],
			mdecoder->codec_context->pix_fmt,
			mdecoder->codec_context->width, mdecoder->codec_context->height);

		mdecoder->decoded_size = avpicture_get_size(mdecoder->codec_context->pix_fmt,
			mdecoder->codec_context->width, mdecoder->codec_context->height);
		mdecoder->decoded_data = malloc(mdecoder->decoded_size);
		ZeroMemory(mdecoder->decoded_data, mdecoder->decoded_size);
		frame = avcodec_alloc_frame();
		avpicture_fill((AVPicture*) frame, mdecoder->decoded_data,
			mdecoder->codec_context->pix_fmt,
			mdecoder->codec_context->width, mdecoder->codec_context->height);

		av_picture_copy((AVPicture*) frame, (AVPicture*) mdecoder->frame,
			mdecoder->codec_context->pix_fmt,
			mdecoder->codec_context->width, mdecoder->codec_context->height);

		av_free(frame);
	}

	return ret;
}

static BOOL tsmf_ffmpeg_decode_audio(ITSMFDecoder* decoder, const BYTE* data, UINT32 data_size, UINT32 extensions)
{
	TSMFFFmpegDecoder* mdecoder = (TSMFFFmpegDecoder*) decoder;
	int len;
	int frame_size;
	UINT32 src_size;
	const BYTE* src;
	BYTE* dst;
	int dst_offset;

#if 0
	LLOGLN(0, ("tsmf_ffmpeg_decode_audio: data_size %d", data_size));
	int i;
	for (i = 0; i < data_size; i++)
	{
		LLOG(0, ("%02X ", data[i]));
		if (i % 16 == 15)
			LLOG(0, ("\n"));
	}
	LLOG(0, ("\n"));
#endif

	if (mdecoder->decoded_size_max == 0)
		mdecoder->decoded_size_max = AVCODEC_MAX_AUDIO_FRAME_SIZE + 16;
	mdecoder->decoded_data = malloc(mdecoder->decoded_size_max);
	ZeroMemory(mdecoder->decoded_data, mdecoder->decoded_size_max);
	/* align the memory for SSE2 needs */
	dst = (BYTE*) (((uintptr_t) mdecoder->decoded_data + 15) & ~ 0x0F);
	dst_offset = dst - mdecoder->decoded_data;
	src = data;
	src_size = data_size;

	while (src_size > 0)
	{
		/* Ensure enough space for decoding */
		if (mdecoder->decoded_size_max - mdecoder->decoded_size < AVCODEC_MAX_AUDIO_FRAME_SIZE)
		{
			mdecoder->decoded_size_max = mdecoder->decoded_size_max * 2 + 16;
			mdecoder->decoded_data = realloc(mdecoder->decoded_data, mdecoder->decoded_size_max);
			dst = (BYTE*) (((uintptr_t)mdecoder->decoded_data + 15) & ~ 0x0F);
			if (dst - mdecoder->decoded_data != dst_offset)
			{
				/* re-align the memory if the alignment has changed after realloc */
				memmove(dst, mdecoder->decoded_data + dst_offset, mdecoder->decoded_size);
				dst_offset = dst - mdecoder->decoded_data;
			}
			dst += mdecoder->decoded_size;
		}
		
		frame_size = mdecoder->decoded_size_max - mdecoder->decoded_size;
#if LIBAVCODEC_VERSION_MAJOR < 52 || (LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR <= 20)
		len = avcodec_decode_audio2(mdecoder->codec_context,
			(int16_t*) dst, &frame_size, src, src_size);
#else
		{
			AVFrame* decoded_frame = avcodec_alloc_frame();
			int got_frame = 0;
			AVPacket pkt;
			av_init_packet(&pkt);
			pkt.data = (BYTE*) src;
			pkt.size = src_size;
			len = avcodec_decode_audio4(mdecoder->codec_context, decoded_frame, &got_frame, &pkt);
			
			if (len >= 0 && got_frame)
			{
				frame_size = av_samples_get_buffer_size(NULL, mdecoder->codec_context->channels,
					decoded_frame->nb_samples, mdecoder->codec_context->sample_fmt, 1);
				memcpy(dst, decoded_frame->data[0], frame_size);
			}
			
			av_free(decoded_frame);
		}
#endif
		if (len <= 0 || frame_size <= 0)
		{
			DEBUG_WARN("error decoding");
			break;
		}

		src += len;
		src_size -= len;
		mdecoder->decoded_size += frame_size;
		dst += frame_size;
	}

	if (mdecoder->decoded_size == 0)
	{
		free(mdecoder->decoded_data);
		mdecoder->decoded_data = NULL;
	}
	else if (dst_offset)
	{
		/* move the aligned decoded data to original place */
		memmove(mdecoder->decoded_data, mdecoder->decoded_data + dst_offset, mdecoder->decoded_size);
	}

	DEBUG_DVC("data_size %d decoded_size %d",
		data_size, mdecoder->decoded_size);

	return TRUE;
}

static BOOL tsmf_ffmpeg_decode(ITSMFDecoder* decoder, const BYTE* data, UINT32 data_size, UINT32 extensions)
{
	TSMFFFmpegDecoder* mdecoder = (TSMFFFmpegDecoder*) decoder;

	if (mdecoder->decoded_data)
	{
		free(mdecoder->decoded_data);
		mdecoder->decoded_data = NULL;
	}
	mdecoder->decoded_size = 0;

	switch (mdecoder->media_type)
	{
		case AVMEDIA_TYPE_VIDEO:
			return tsmf_ffmpeg_decode_video(decoder, data, data_size, extensions);
		case AVMEDIA_TYPE_AUDIO:
			return tsmf_ffmpeg_decode_audio(decoder, data, data_size, extensions);
		default:
			DEBUG_WARN("unknown media type.");
			return FALSE;
	}
}

static BYTE* tsmf_ffmpeg_get_decoded_data(ITSMFDecoder* decoder, UINT32* size)
{
	BYTE* buf;
	TSMFFFmpegDecoder* mdecoder = (TSMFFFmpegDecoder*) decoder;

	*size = mdecoder->decoded_size;
	buf = mdecoder->decoded_data;
	mdecoder->decoded_data = NULL;
	mdecoder->decoded_size = 0;

	return buf;
}

static UINT32 tsmf_ffmpeg_get_decoded_format(ITSMFDecoder* decoder)
{
	TSMFFFmpegDecoder* mdecoder = (TSMFFFmpegDecoder*) decoder;

	switch (mdecoder->codec_context->pix_fmt)
	{
		case PIX_FMT_YUV420P:
			return RDP_PIXFMT_I420;

		default:
			DEBUG_WARN("unsupported pixel format %u",
				mdecoder->codec_context->pix_fmt);
			return (UINT32) -1;
	}
}

static BOOL tsmf_ffmpeg_get_decoded_dimension(ITSMFDecoder* decoder, UINT32* width, UINT32* height)
{
	TSMFFFmpegDecoder* mdecoder = (TSMFFFmpegDecoder*) decoder;

	if (mdecoder->codec_context->width > 0 && mdecoder->codec_context->height > 0)
	{
		*width = mdecoder->codec_context->width;
		*height = mdecoder->codec_context->height;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static void tsmf_ffmpeg_free(ITSMFDecoder* decoder)
{
	TSMFFFmpegDecoder* mdecoder = (TSMFFFmpegDecoder*) decoder;

	if (mdecoder->frame)
		av_free(mdecoder->frame);

	if (mdecoder->decoded_data)
		free(mdecoder->decoded_data);

	if (mdecoder->codec_context)
	{
		if (mdecoder->prepared)
			avcodec_close(mdecoder->codec_context);
		if (mdecoder->codec_context->extradata)
			free(mdecoder->codec_context->extradata);
		av_free(mdecoder->codec_context);
	}

	free(decoder);
}

static BOOL initialized = FALSE;

#ifdef STATIC_CHANNELS
#define freerdp_tsmf_client_decoder_subsystem_entry	ffmpeg_freerdp_tsmf_client_decoder_subsystem_entry
#endif

ITSMFDecoder* freerdp_tsmf_client_decoder_subsystem_entry(void)
{
	TSMFFFmpegDecoder* decoder;

	if (!initialized)
	{
		avcodec_register_all();
		initialized = TRUE;
	}

	printf("TSMFDecoderEntry FFMPEG\n");

	decoder = (TSMFFFmpegDecoder*) malloc(sizeof(TSMFFFmpegDecoder));
	ZeroMemory(decoder, sizeof(TSMFFFmpegDecoder));

	decoder->iface.SetFormat = tsmf_ffmpeg_set_format;
	decoder->iface.Decode = tsmf_ffmpeg_decode;
	decoder->iface.GetDecodedData = tsmf_ffmpeg_get_decoded_data;
	decoder->iface.GetDecodedFormat = tsmf_ffmpeg_get_decoded_format;
	decoder->iface.GetDecodedDimension = tsmf_ffmpeg_get_decoded_dimension;
	decoder->iface.Free = tsmf_ffmpeg_free;

	return (ITSMFDecoder*) decoder;
}
