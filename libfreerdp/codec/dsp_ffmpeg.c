/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Digital Sound Processing - FFMPEG backend
 *
 * Copyright 2018 Armin Novak <armin.novak@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
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

#include <freerdp/log.h>

#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#if defined(SWRESAMPLE_FOUND)
#include <libswresample/swresample.h>
#elif defined(AVRESAMPLE_FOUND)
#include <libavresample/avresample.h>
#else
#error "libswresample or libavresample required"
#endif

#include "dsp.h"
#include "dsp_ffmpeg.h"

#define TAG FREERDP_TAG("dsp.ffmpeg")

struct _FREERDP_DSP_CONTEXT
{
	AUDIO_FORMAT format;

	BOOL isOpen;
	BOOL encoder;

	UINT32 bufferedSamples;

	enum AVCodecID id;
	AVCodec* codec;
	AVCodecContext* context;
	AVFrame* frame;
	AVFrame* resampled;
	AVFrame* buffered;
	AVPacket* packet;
#if defined(SWRESAMPLE_FOUND)
	SwrContext* rcontext;
#else
	AVAudioResampleContext* rcontext;
#endif
};

static BOOL ffmpeg_codec_is_filtered(enum AVCodecID id, BOOL encoder)
{
	switch (id)
	{
#if !defined(WITH_DSP_EXPERIMENTAL)

		case AV_CODEC_ID_ADPCM_IMA_OKI:
		case AV_CODEC_ID_MP3:
		case AV_CODEC_ID_ADPCM_MS:
		case AV_CODEC_ID_G723_1:
			return TRUE;
#endif

		case AV_CODEC_ID_NONE:
			return TRUE;

		default:
			return FALSE;
	}
}

static enum AVCodecID ffmpeg_get_avcodec(const AUDIO_FORMAT* format)
{
	const char* id;

	if (!format)
		return AV_CODEC_ID_NONE;

	id = audio_format_get_tag_string(format->wFormatTag);

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_UNKNOWN:
			return AV_CODEC_ID_NONE;

		case WAVE_FORMAT_PCM:
			switch (format->wBitsPerSample)
			{
				case 16:
					return AV_CODEC_ID_PCM_U16LE;

				case 8:
					return AV_CODEC_ID_PCM_U8;

				default:
					return AV_CODEC_ID_NONE;
			}

		case WAVE_FORMAT_DVI_ADPCM:
			return AV_CODEC_ID_ADPCM_IMA_OKI;

		case WAVE_FORMAT_ADPCM:
			return AV_CODEC_ID_ADPCM_MS;

		case WAVE_FORMAT_ALAW:
			return AV_CODEC_ID_PCM_ALAW;

		case WAVE_FORMAT_MULAW:
			return AV_CODEC_ID_PCM_MULAW;

		case WAVE_FORMAT_GSM610:
			return AV_CODEC_ID_GSM_MS;

		case WAVE_FORMAT_MSG723:
			return AV_CODEC_ID_G723_1;

		case WAVE_FORMAT_AAC_MS:
			return AV_CODEC_ID_AAC;

		default:
			return AV_CODEC_ID_NONE;
	}
}

static int ffmpeg_sample_format(const AUDIO_FORMAT* format)
{
	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			switch (format->wBitsPerSample)
			{
				case 8:
					return AV_SAMPLE_FMT_U8;

				case 16:
					return AV_SAMPLE_FMT_S16;

				default:
					return FALSE;
			}

		case WAVE_FORMAT_DVI_ADPCM:
		case WAVE_FORMAT_ADPCM:
			return AV_SAMPLE_FMT_S16P;

		case WAVE_FORMAT_MPEGLAYER3:
		case WAVE_FORMAT_AAC_MS:
			return AV_SAMPLE_FMT_FLTP;

		case WAVE_FORMAT_MSG723:
		case WAVE_FORMAT_GSM610:
			return AV_SAMPLE_FMT_S16P;

		case WAVE_FORMAT_ALAW:
			return AV_SAMPLE_FMT_S16;

		default:
			return FALSE;
	}
}

static void ffmpeg_close_context(FREERDP_DSP_CONTEXT* context)
{
	if (context)
	{
		if (context->context)
			avcodec_free_context(&context->context);

		if (context->frame)
			av_frame_free(&context->frame);

		if (context->resampled)
			av_frame_free(&context->resampled);

		if (context->buffered)
			av_frame_free(&context->buffered);

		if (context->packet)
			av_packet_free(&context->packet);

		if (context->rcontext)
		{
#if defined(SWRESAMPLE_FOUND)
			swr_free(&context->rcontext);
#else
			avresample_free(&context->rcontext);
#endif
		}

		context->id = AV_CODEC_ID_NONE;
		context->codec = NULL;
		context->isOpen = FALSE;
		context->context = NULL;
		context->frame = NULL;
		context->resampled = NULL;
		context->packet = NULL;
		context->rcontext = NULL;
	}
}

static BOOL ffmpeg_open_context(FREERDP_DSP_CONTEXT* context)
{
	int ret;
	int layout;
	const AUDIO_FORMAT* format;

	if (!context || context->isOpen)
		return FALSE;

	format = &context->format;

	if (!format)
		return FALSE;

	layout = av_get_default_channel_layout(format->nChannels);
	context->id = ffmpeg_get_avcodec(format);

	if (ffmpeg_codec_is_filtered(context->id, context->encoder))
		goto fail;

	if (context->encoder)
		context->codec = avcodec_find_encoder(context->id);
	else
		context->codec = avcodec_find_decoder(context->id);

	if (!context->codec)
		goto fail;

	context->context = avcodec_alloc_context3(context->codec);

	if (!context->context)
		goto fail;

	switch (context->id)
	{
		/* We need support for multichannel and sample rates != 8000 */
		case AV_CODEC_ID_GSM_MS:
			context->context->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;
			break;

		case AV_CODEC_ID_AAC:
			context->context->profile = FF_PROFILE_AAC_MAIN;
			break;

		default:
			break;
	}

	context->context->channels = format->nChannels;
	context->context->channel_layout = layout;
	context->context->sample_rate = format->nSamplesPerSec;
	context->context->block_align = format->nBlockAlign;
	context->context->bit_rate = format->nAvgBytesPerSec * 8;
	context->context->sample_fmt = ffmpeg_sample_format(format);
	context->context->time_base = av_make_q(1, context->context->sample_rate);

	if ((ret = avcodec_open2(context->context, context->codec, NULL)) < 0)
	{
		const char* err = av_err2str(ret);
		WLog_ERR(TAG, "Error avcodec_open2 %s [%d]", err, ret);
		goto fail;
	}

	context->packet = av_packet_alloc();

	if (!context->packet)
		goto fail;

	context->frame = av_frame_alloc();

	if (!context->frame)
		goto fail;

	context->resampled = av_frame_alloc();

	if (!context->resampled)
		goto fail;

	context->buffered = av_frame_alloc();

	if (!context->buffered)
		goto fail;

#if defined(SWRESAMPLE_FOUND)
	context->rcontext = swr_alloc();
#else
	context->rcontext = avresample_alloc_context();
#endif

	if (!context->rcontext)
		goto fail;

	context->frame->channel_layout = layout;
	context->frame->channels = format->nChannels;
	context->frame->sample_rate = format->nSamplesPerSec;
	context->frame->format = AV_SAMPLE_FMT_S16;

	if (context->encoder)
	{
		context->resampled->format = context->context->sample_fmt;
		context->resampled->sample_rate = context->context->sample_rate;
	}
	else
	{
		context->resampled->format = AV_SAMPLE_FMT_S16;
		context->resampled->sample_rate = format->nSamplesPerSec;
	}

	context->resampled->channel_layout = layout;
	context->resampled->channels = format->nChannels;

	if (context->context->frame_size > 0)
	{
		context->buffered->channel_layout = context->resampled->channel_layout;
		context->buffered->channels = context->resampled->channels;
		context->buffered->format = context->resampled->format;
		context->buffered->nb_samples = context->context->frame_size;

		if ((ret = av_frame_get_buffer(context->buffered, 1)) < 0)
			goto fail;
	}

	context->isOpen = TRUE;
	return TRUE;
fail:
	ffmpeg_close_context(context);
	return FALSE;
}

#if defined(SWRESAMPLE_FOUND)
static BOOL ffmpeg_resample_frame(SwrContext* context, AVFrame* in, AVFrame* out)
{
	int ret;

	if (!swr_is_initialized(context))
	{
		if ((ret = swr_config_frame(context, out, in)) < 0)
		{
			const char* err = av_err2str(ret);
			WLog_ERR(TAG, "Error during resampling %s [%d]", err, ret);
			return FALSE;
		}

		if ((ret = (swr_init(context))) < 0)
		{
			const char* err = av_err2str(ret);
			WLog_ERR(TAG, "Error during resampling %s [%d]", err, ret);
			return FALSE;
		}
	}

	if ((ret = swr_convert_frame(context, out, in)) < 0)
	{
		const char* err = av_err2str(ret);
		WLog_ERR(TAG, "Error during resampling %s [%d]", err, ret);
		return FALSE;
	}

	return TRUE;
}
#else
static BOOL ffmpeg_resample_frame(AVAudioResampleContext* context, AVFrame* in, AVFrame* out)
{
	int ret;

	if (!avresample_is_open(context))
	{
		if ((ret = avresample_config(context, out, in)) < 0)
		{
			const char* err = av_err2str(ret);
			WLog_ERR(TAG, "Error during resampling %s [%d]", err, ret);
			return FALSE;
		}

		if ((ret = (avresample_open(context))) < 0)
		{
			const char* err = av_err2str(ret);
			WLog_ERR(TAG, "Error during resampling %s [%d]", err, ret);
			return FALSE;
		}
	}

	if ((ret = avresample_convert_frame(context, out, in)) < 0)
	{
		const char* err = av_err2str(ret);
		WLog_ERR(TAG, "Error during resampling %s [%d]", err, ret);
		return FALSE;
	}

	return TRUE;
}
#endif

static BOOL ffmpeg_encode_frame(AVCodecContext* context, AVFrame* in, AVPacket* packet,
                                wStream* out)
{
	int ret;
	/* send the packet with the compressed data to the encoder */
	ret = avcodec_send_frame(context, in);

	if (ret < 0)
	{
		const char* err = av_err2str(ret);
		WLog_ERR(TAG, "Error submitting the packet to the encoder %s [%d]", err, ret);
		return FALSE;
	}

	/* read all the output frames (in general there may be any number of them */
	while (ret >= 0)
	{
		ret = avcodec_receive_packet(context, packet);

		if ((ret == AVERROR(EAGAIN)) || (ret == AVERROR_EOF))
			return TRUE;
		else if (ret < 0)
		{
			const char* err = av_err2str(ret);
			WLog_ERR(TAG, "Error during encoding %s [%d]", err, ret);
			return FALSE;
		}

		if (!Stream_EnsureRemainingCapacity(out, packet->size))
			return FALSE;

		Stream_Write(out, packet->data, packet->size);
		av_packet_unref(packet);
	}

	return TRUE;
}

static BOOL ffmpeg_fill_frame(AVFrame* frame, const AUDIO_FORMAT* inputFormat, const BYTE* data,
                              size_t size)
{
	int ret, bpp;
	frame->channels = inputFormat->nChannels;
	frame->sample_rate = inputFormat->nSamplesPerSec;
	frame->format = ffmpeg_sample_format(inputFormat);
	frame->channel_layout = av_get_default_channel_layout(frame->channels);
	bpp = av_get_bytes_per_sample(frame->format);
	frame->nb_samples = size / inputFormat->nChannels / bpp;

	if ((ret = avcodec_fill_audio_frame(frame, frame->channels, frame->format, data, size, 1)) < 0)
	{
		const char* err = av_err2str(ret);
		WLog_ERR(TAG, "Error during audio frame fill %s [%d]", err, ret);
		return FALSE;
	}

	return TRUE;
}
#if defined(SWRESAMPLE_FOUND)
static BOOL ffmpeg_decode(AVCodecContext* dec_ctx, AVPacket* pkt, AVFrame* frame,
                          SwrContext* resampleContext, AVFrame* resampled, wStream* out)
#else
static BOOL ffmpeg_decode(AVCodecContext* dec_ctx, AVPacket* pkt, AVFrame* frame,
                          AVAudioResampleContext* resampleContext, AVFrame* resampled, wStream* out)
#endif
{
	int ret;
	/* send the packet with the compressed data to the decoder */
	ret = avcodec_send_packet(dec_ctx, pkt);

	if (ret < 0)
	{
		const char* err = av_err2str(ret);
		WLog_ERR(TAG, "Error submitting the packet to the decoder %s [%d]", err, ret);
		return FALSE;
	}

	/* read all the output frames (in general there may be any number of them */
	while (ret >= 0)
	{
		ret = avcodec_receive_frame(dec_ctx, frame);

		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return TRUE;
		else if (ret < 0)
		{
			const char* err = av_err2str(ret);
			WLog_ERR(TAG, "Error during decoding %s [%d]", err, ret);
			return FALSE;
		}

#if defined(SWRESAMPLE_FOUND)
		if (!swr_is_initialized(resampleContext))
		{
			if ((ret = swr_config_frame(resampleContext, resampled, frame)) < 0)
			{
#else
		if (!avresample_is_open(resampleContext))
		{
			if ((ret = avresample_config(resampleContext, resampled, frame)) < 0)
			{
#endif
				const char* err = av_err2str(ret);
				WLog_ERR(TAG, "Error during resampling %s [%d]", err, ret);
				return FALSE;
			}

#if defined(SWRESAMPLE_FOUND)
			if ((ret = (swr_init(resampleContext))) < 0)
#else
			if ((ret = (avresample_open(resampleContext))) < 0)
#endif
			{
				const char* err = av_err2str(ret);
				WLog_ERR(TAG, "Error during resampling %s [%d]", err, ret);
				return FALSE;
			}
		}

#if defined(SWRESAMPLE_FOUND)
		if ((ret = swr_convert_frame(resampleContext, resampled, frame)) < 0)
#else
		if ((ret = avresample_convert_frame(resampleContext, resampled, frame)) < 0)
#endif
		{
			const char* err = av_err2str(ret);
			WLog_ERR(TAG, "Error during resampling %s [%d]", err, ret);
			return FALSE;
		}

		{
			const size_t data_size = resampled->channels * resampled->nb_samples * 2;
			Stream_EnsureRemainingCapacity(out, data_size);
			Stream_Write(out, resampled->data[0], data_size);
		}
	}

	return TRUE;
}

BOOL freerdp_dsp_ffmpeg_supports_format(const AUDIO_FORMAT* format, BOOL encode)
{
	enum AVCodecID id = ffmpeg_get_avcodec(format);

	if (ffmpeg_codec_is_filtered(id, encode))
		return FALSE;

	if (encode)
		return avcodec_find_encoder(id) != NULL;
	else
		return avcodec_find_decoder(id) != NULL;
}

FREERDP_DSP_CONTEXT* freerdp_dsp_ffmpeg_context_new(BOOL encode)
{
	FREERDP_DSP_CONTEXT* context;
	avcodec_register_all();
	context = calloc(1, sizeof(FREERDP_DSP_CONTEXT));

	if (!context)
		return NULL;

	context->encoder = encode;
	return context;
}

void freerdp_dsp_ffmpeg_context_free(FREERDP_DSP_CONTEXT* context)
{
	if (context)
	{
		ffmpeg_close_context(context);
		free(context);
	}
}

BOOL freerdp_dsp_ffmpeg_context_reset(FREERDP_DSP_CONTEXT* context,
                                      const AUDIO_FORMAT* targetFormat)
{
	if (!context || !targetFormat)
		return FALSE;

	ffmpeg_close_context(context);
	context->format = *targetFormat;
	return ffmpeg_open_context(context);
}

BOOL freerdp_dsp_ffmpeg_encode(FREERDP_DSP_CONTEXT* context, const AUDIO_FORMAT* format,
                               const BYTE* data, size_t length, wStream* out)
{
	int rc;

	if (!context || !format || !data || !out || !context->encoder)
		return FALSE;

	if (!context || !data || !out)
		return FALSE;

	/* Create input frame */
	if (!ffmpeg_fill_frame(context->frame, format, data, length))
		return FALSE;

	/* Resample to desired format. */
	if (!ffmpeg_resample_frame(context->rcontext, context->frame, context->resampled))
		return FALSE;

	if (context->context->frame_size <= 0)
	{
		return ffmpeg_encode_frame(context->context, context->resampled, context->packet, out);
	}
	else
	{
		int copied = 0;
		int rest = context->resampled->nb_samples;

		do
		{
			int inSamples = rest;

			if ((inSamples < 0) || (context->bufferedSamples > (UINT32)(INT_MAX - inSamples)))
				return FALSE;

			if (inSamples + (int)context->bufferedSamples > context->context->frame_size)
				inSamples = context->context->frame_size - (int)context->bufferedSamples;

			rc =
			    av_samples_copy(context->buffered->extended_data, context->resampled->extended_data,
			                    (int)context->bufferedSamples, copied, inSamples,
			                    context->context->channels, context->context->sample_fmt);
			rest -= inSamples;
			copied += inSamples;
			context->bufferedSamples += (UINT32)inSamples;

			if (context->context->frame_size <= (int)context->bufferedSamples)
			{
				/* Encode in desired format. */
				if (!ffmpeg_encode_frame(context->context, context->buffered, context->packet, out))
					return FALSE;

				context->bufferedSamples = 0;
			}
		} while (rest > 0);

		return TRUE;
	}
}

BOOL freerdp_dsp_ffmpeg_decode(FREERDP_DSP_CONTEXT* context, const AUDIO_FORMAT* srcFormat,
                               const BYTE* data, size_t length, wStream* out)
{
	if (!context || !srcFormat || !data || !out || context->encoder)
		return FALSE;

	av_init_packet(context->packet);
	context->packet->data = (uint8_t*)data;
	context->packet->size = length;
	return ffmpeg_decode(context->context, context->packet, context->frame, context->rcontext,
	                     context->resampled, out);
}
