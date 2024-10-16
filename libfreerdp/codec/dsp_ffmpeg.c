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

#include <freerdp/config.h>

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

struct S_FREERDP_DSP_CONTEXT
{
	FREERDP_DSP_COMMON_CONTEXT common;

	BOOL isOpen;

	UINT32 bufferedSamples;

	enum AVCodecID id;
	const AVCodec* codec;
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
		case AV_CODEC_ID_GSM_MS:
		case AV_CODEC_ID_PCM_ALAW:
		case AV_CODEC_ID_PCM_MULAW:
			return TRUE;
#endif

		case AV_CODEC_ID_NONE:
			return TRUE;

		case AV_CODEC_ID_AAC:
		case AV_CODEC_ID_AAC_LATM:
			return FALSE;

		default:
			return FALSE;
	}
}

static enum AVCodecID ffmpeg_get_avcodec(const AUDIO_FORMAT* WINPR_RESTRICT format)
{
	if (!format)
		return AV_CODEC_ID_NONE;

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

		case WAVE_FORMAT_OPUS:
			return AV_CODEC_ID_OPUS;

		default:
			return AV_CODEC_ID_NONE;
	}
}

static int ffmpeg_sample_format(const AUDIO_FORMAT* WINPR_RESTRICT format)
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

		case WAVE_FORMAT_OPUS:
			return AV_SAMPLE_FMT_S16;

		case WAVE_FORMAT_MSG723:
		case WAVE_FORMAT_GSM610:
			return AV_SAMPLE_FMT_S16P;

		case WAVE_FORMAT_ALAW:
			return AV_SAMPLE_FMT_S16;

		default:
			return FALSE;
	}
}

static void ffmpeg_close_context(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context)
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

static BOOL ffmpeg_open_context(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context)
{
	int ret = 0;

	if (!context || context->isOpen)
		return FALSE;

	const AUDIO_FORMAT* format = &context->common.format;

	if (!format)
		return FALSE;
	context->id = ffmpeg_get_avcodec(format);

	if (ffmpeg_codec_is_filtered(context->id, context->common.encoder))
		goto fail;

	if (context->common.encoder)
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

	context->context->max_b_frames = 1;
	context->context->delay = 0;

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
	av_channel_layout_default(&context->context->ch_layout, format->nChannels);
#else
	context->context->channels = format->nChannels;
	const int64_t layout = av_get_default_channel_layout(format->nChannels);
	context->context->channel_layout = layout;
#endif
	context->context->sample_rate = (int)format->nSamplesPerSec;
	context->context->block_align = format->nBlockAlign;
	context->context->bit_rate = format->nAvgBytesPerSec * 8LL;
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

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
	av_channel_layout_default(&context->frame->ch_layout, format->nChannels);
#else
	context->frame->channel_layout = layout;
	context->frame->channels = format->nChannels;
#endif
	WINPR_ASSERT(format->nSamplesPerSec <= INT_MAX);
	context->frame->sample_rate = (int)format->nSamplesPerSec;
	context->frame->format = AV_SAMPLE_FMT_S16;

	if (context->common.encoder)
	{
		context->resampled->format = context->context->sample_fmt;
		context->resampled->sample_rate = context->context->sample_rate;
	}
	else
	{
		context->resampled->format = AV_SAMPLE_FMT_S16;

		WINPR_ASSERT(format->nSamplesPerSec <= INT_MAX);
		context->resampled->sample_rate = (int)format->nSamplesPerSec;
	}

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
	av_channel_layout_default(&context->resampled->ch_layout, format->nChannels);
#else
	context->resampled->channel_layout = layout;
	context->resampled->channels = format->nChannels;
#endif

	if (context->context->frame_size > 0)
	{
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
		ret = av_channel_layout_copy(&context->buffered->ch_layout, &context->resampled->ch_layout);
		if (ret != 0)
			goto fail;
#else
		context->buffered->channel_layout = context->resampled->channel_layout;
		context->buffered->channels = context->resampled->channels;
#endif
		context->buffered->format = context->resampled->format;
		context->buffered->nb_samples = context->context->frame_size;

		ret = av_frame_get_buffer(context->buffered, 1);
		if (ret < 0)
			goto fail;
	}

	context->isOpen = TRUE;
	return TRUE;
fail:
	ffmpeg_close_context(context);
	return FALSE;
}

#if defined(SWRESAMPLE_FOUND)
static BOOL ffmpeg_resample_frame(SwrContext* WINPR_RESTRICT context, AVFrame* WINPR_RESTRICT in,
                                  AVFrame* WINPR_RESTRICT out)
{
	int ret = 0;

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
static BOOL ffmpeg_resample_frame(AVAudioResampleContext* WINPR_RESTRICT context,
                                  AVFrame* WINPR_RESTRICT in, AVFrame* WINPR_RESTRICT out)
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

static BOOL ffmpeg_encode_frame(AVCodecContext* WINPR_RESTRICT context, AVFrame* WINPR_RESTRICT in,
                                AVPacket* WINPR_RESTRICT packet, wStream* WINPR_RESTRICT out)
{
	if (in->format == AV_SAMPLE_FMT_FLTP)
	{
		uint8_t** pp = in->extended_data;
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(57, 28, 100)
		const int nr_channels = in->channels;
#else
		const int nr_channels = in->ch_layout.nb_channels;
#endif

		for (int y = 0; y < nr_channels; y++)
		{
			float* data = (float*)pp[y];
			for (int x = 0; x < in->nb_samples; x++)
			{
				const float val1 = data[x];
				if (isnan(val1))
					data[x] = 0.0f;
				else if (isinf(val1))
				{
					if (val1 < 0.0f)
						data[x] = -1.0f;
					else
						data[x] = 1.0f;
				}
			}
		}
	}
	/* send the packet with the compressed data to the encoder */
	int ret = avcodec_send_frame(context, in);

	if (ret < 0)
	{
		const char* err = av_err2str(ret);
		WLog_ERR(TAG, "Error submitting the packet to the encoder %s [%d]", err, ret);
		return FALSE;
	}

	/* read all the output frames (in general there may be any number of them */
	while (TRUE)
	{
		ret = avcodec_receive_packet(context, packet);

		if ((ret == AVERROR(EAGAIN)) || (ret == AVERROR_EOF))
			break;

		if (ret < 0)
		{
			const char* err = av_err2str(ret);
			WLog_ERR(TAG, "Error during encoding %s [%d]", err, ret);
			return FALSE;
		}

		WINPR_ASSERT(packet->size >= 0);
		if (!Stream_EnsureRemainingCapacity(out, (size_t)packet->size))
			return FALSE;

		Stream_Write(out, packet->data, (size_t)packet->size);
		av_packet_unref(packet);
	}

	return TRUE;
}

static BOOL ffmpeg_fill_frame(AVFrame* WINPR_RESTRICT frame,
                              const AUDIO_FORMAT* WINPR_RESTRICT inputFormat,
                              const BYTE* WINPR_RESTRICT data, size_t size)
{
	int ret = 0;
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(57, 28, 100)
	frame->channels = inputFormat->nChannels;
	frame->channel_layout = av_get_default_channel_layout(frame->channels);
#else
	av_channel_layout_default(&frame->ch_layout, inputFormat->nChannels);
#endif
	WINPR_ASSERT(inputFormat->nSamplesPerSec <= INT_MAX);
	frame->sample_rate = (int)inputFormat->nSamplesPerSec;
	frame->format = ffmpeg_sample_format(inputFormat);

	const int bpp = av_get_bytes_per_sample(frame->format);
	WINPR_ASSERT(bpp >= 0);
	WINPR_ASSERT(size <= INT_MAX);
	const size_t nb_samples = size / inputFormat->nChannels / (size_t)bpp;
	frame->nb_samples = (int)nb_samples;

	if ((ret = avcodec_fill_audio_frame(frame, inputFormat->nChannels, frame->format, data,
	                                    (int)size, 1)) < 0)
	{
		const char* err = av_err2str(ret);
		WLog_ERR(TAG, "Error during audio frame fill %s [%d]", err, ret);
		return FALSE;
	}

	return TRUE;
}
#if defined(SWRESAMPLE_FOUND)
static BOOL ffmpeg_decode(AVCodecContext* WINPR_RESTRICT dec_ctx, AVPacket* WINPR_RESTRICT pkt,
                          AVFrame* WINPR_RESTRICT frame, SwrContext* WINPR_RESTRICT resampleContext,
                          AVFrame* WINPR_RESTRICT resampled, wStream* WINPR_RESTRICT out)
#else
static BOOL ffmpeg_decode(AVCodecContext* dec_ctx, AVPacket* pkt, AVFrame* frame,
                          AVAudioResampleContext* resampleContext, AVFrame* resampled, wStream* out)
#endif
{
	int ret = 0;
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

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
			WINPR_ASSERT(resampled->ch_layout.nb_channels >= 0);
			const size_t nrchannels = (size_t)resampled->ch_layout.nb_channels;
#else
			const size_t nrchannels = resampled->channels;
#endif
			WINPR_ASSERT(resampled->nb_samples >= 0);
			const size_t data_size = nrchannels * (size_t)resampled->nb_samples * 2ull;
			if (!Stream_EnsureRemainingCapacity(out, data_size))
				return FALSE;
			Stream_Write(out, resampled->data[0], data_size);
		}
	}

	return TRUE;
}

BOOL freerdp_dsp_ffmpeg_supports_format(const AUDIO_FORMAT* WINPR_RESTRICT format, BOOL encode)
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
	FREERDP_DSP_CONTEXT* context = NULL;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 10, 100)
	avcodec_register_all();
#endif
	context = calloc(1, sizeof(FREERDP_DSP_CONTEXT));

	if (!context)
		goto fail;

	if (!freerdp_dsp_common_context_init(&context->common, encode))
		goto fail;

	return context;

fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	freerdp_dsp_ffmpeg_context_free(context);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

void freerdp_dsp_ffmpeg_context_free(FREERDP_DSP_CONTEXT* context)
{
	if (context)
	{
		ffmpeg_close_context(context);
		freerdp_dsp_common_context_uninit(&context->common);
		free(context);
	}
}

BOOL freerdp_dsp_ffmpeg_context_reset(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                      const AUDIO_FORMAT* WINPR_RESTRICT targetFormat)
{
	if (!context || !targetFormat)
		return FALSE;

	ffmpeg_close_context(context);
	context->common.format = *targetFormat;
	return ffmpeg_open_context(context);
}

static BOOL freerdp_dsp_channel_mix(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                    const BYTE* WINPR_RESTRICT src, size_t size,
                                    const AUDIO_FORMAT* WINPR_RESTRICT srcFormat,
                                    const BYTE** WINPR_RESTRICT data, size_t* WINPR_RESTRICT length,
                                    AUDIO_FORMAT* WINPR_RESTRICT dstFormat)
{
	UINT32 bpp = 0;
	size_t samples = 0;

	if (!context || !data || !length || !dstFormat)
		return FALSE;

	if (srcFormat->wFormatTag != WAVE_FORMAT_PCM)
		return FALSE;

	bpp = srcFormat->wBitsPerSample > 8 ? 2 : 1;
	samples = size / bpp / srcFormat->nChannels;

	*dstFormat = *srcFormat;
	if (context->common.format.nChannels == srcFormat->nChannels)
	{
		*data = src;
		*length = size;
		return TRUE;
	}

	Stream_SetPosition(context->common.channelmix, 0);

	/* Destination has more channels than source */
	if (context->common.format.nChannels > srcFormat->nChannels)
	{
		switch (srcFormat->nChannels)
		{
			case 1:
				if (!Stream_EnsureCapacity(context->common.channelmix, size * 2))
					return FALSE;

				for (size_t x = 0; x < samples; x++)
				{
					for (size_t y = 0; y < bpp; y++)
						Stream_Write_UINT8(context->common.channelmix, src[x * bpp + y]);

					for (size_t y = 0; y < bpp; y++)
						Stream_Write_UINT8(context->common.channelmix, src[x * bpp + y]);
				}

				Stream_SealLength(context->common.channelmix);
				*data = Stream_Buffer(context->common.channelmix);
				*length = Stream_Length(context->common.channelmix);
				dstFormat->nChannels = 2;
				return TRUE;

			case 2:  /* We only support stereo, so we can not handle this case. */
			default: /* Unsupported number of channels */
				WLog_WARN(TAG, "[%s] unsuported source channel count %" PRIu16, __func__,
				          srcFormat->nChannels);
				return FALSE;
		}
	}

	/* Destination has less channels than source */
	switch (srcFormat->nChannels)
	{
		case 2:
			if (!Stream_EnsureCapacity(context->common.channelmix, size / 2))
				return FALSE;

			/* Simply drop second channel.
			 * TODO: Calculate average */
			for (size_t x = 0; x < samples; x++)
			{
				for (size_t y = 0; y < bpp; y++)
					Stream_Write_UINT8(context->common.channelmix, src[2 * x * bpp + y]);
			}

			Stream_SealLength(context->common.channelmix);
			*data = Stream_Buffer(context->common.channelmix);
			*length = Stream_Length(context->common.channelmix);
			dstFormat->nChannels = 1;
			return TRUE;

		case 1:  /* Invalid, do we want to use a 0 channel sound? */
		default: /* Unsupported number of channels */
			WLog_WARN(TAG, "[%s] unsuported channel count %" PRIu16, __func__,
			          srcFormat->nChannels);
			return FALSE;
	}

	return FALSE;
}

BOOL freerdp_dsp_ffmpeg_encode(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                               const AUDIO_FORMAT* WINPR_RESTRICT format,
                               const BYTE* WINPR_RESTRICT sdata, size_t length,
                               wStream* WINPR_RESTRICT out)
{
	AUDIO_FORMAT fmt = { 0 };

	if (!context || !format || !sdata || !out || !context->common.encoder)
		return FALSE;

	if (!context || !sdata || !out)
		return FALSE;

	/* https://github.com/FreeRDP/FreeRDP/issues/7607
	 *
	 * we get noisy data with channel transformation, so do it ourselves.
	 */
	const BYTE* data = NULL;
	if (!freerdp_dsp_channel_mix(context, sdata, length, format, &data, &length, &fmt))
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

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
			const int nrchannels = context->context->ch_layout.nb_channels;
#else
			const int nrchannels = context->context->channels;
#endif
			const int rc =
			    av_samples_copy(context->buffered->extended_data, context->resampled->extended_data,
			                    (int)context->bufferedSamples, copied, inSamples, nrchannels,
			                    context->context->sample_fmt);
			if (rc < 0)
				return FALSE;
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

BOOL freerdp_dsp_ffmpeg_decode(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                               const AUDIO_FORMAT* WINPR_RESTRICT srcFormat,
                               const BYTE* WINPR_RESTRICT data, size_t length,
                               wStream* WINPR_RESTRICT out)
{
	if (!context || !srcFormat || !data || !out || context->common.encoder)
		return FALSE;

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 133, 100)
	av_init_packet(context->packet);
#endif
	context->packet->data = WINPR_CAST_CONST_PTR_AWAY(data, uint8_t*);

	WINPR_ASSERT(length <= INT_MAX);
	context->packet->size = (int)length;
	return ffmpeg_decode(context->context, context->packet, context->frame, context->rcontext,
	                     context->resampled, out);
}
