/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Digital Sound Processing - Android MediaCodec AAC backend
 *
 * Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>
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

#include "dsp_mediacodec.h"

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <freerdp/log.h>

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>

#define TAG FREERDP_TAG("dsp.mediacodec")

static const char* AAC_MIME = "audio/mp4a-latm";
static const int AAC_OBJECT_LC = 2; /* AAC-LC */

/* AMEDIAFORMAT_KEY_CSD_0 has no NDK constant below API 28; the string works at runtime */
static const char* MEDIACODEC_KEY_CSD_0 = "csd-0";

static const int64_t MEDIACODEC_DSP_DEQUEUE_TIMEOUT = 10000; /* microseconds */
static const int MEDIACODEC_DSP_MAX_RETRIES = 100;
static const size_t AAC_FRAME_SAMPLES = 1024;
static const int32_t DECODER_MAX_INPUT_SIZE = 65536;
static const UINT32 AAC_MAX_SAMPLE_RATE = 96000;

typedef struct
{
	AMediaCodec* codec;
	BOOL started;
	BOOL isEncoder;
	UINT32 sampleRate;
	UINT16 channels;
	uint64_t sampleCount;

	/* Encoder PCM accumulation buffer */
	BYTE* encBuffer;
	size_t encBufferedBytes;
	size_t encFrameBytes; /* 1024 * channels * sizeof(int16_t) */

	/* Decoder detected output format */
	UINT32 outSampleRate;
	UINT16 outChannels;
	BOOL formatMismatchLogged;
	BOOL srcMismatchLogged;
} MEDIACODEC_AAC;

#define mediacodec_aac_warn(status, what) \
	mediacodec_aac_warn_((status), (what), __FILE__, __func__, __LINE__)

static void mediacodec_aac_warn_(media_status_t status, const char* what, const char* file,
                                 const char* fkt, size_t line)
{
	if (status == AMEDIA_OK)
		return;

	wLog* log = WLog_Get(TAG);
	if (WLog_IsLevelActive(log, WLOG_WARN))
		WLog_PrintTextMessage(log, WLOG_WARN, line, file, fkt, "%s failed: %d", what, status);
}

/* MPEG-4 sampling frequency index table */
WINPR_ATTR_NODISCARD
static int aac_sample_rate_index(UINT32 rate)
{
	static const UINT32 rates[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000,
		                            22050, 16000, 12000, 11025, 8000,  7350 };
	for (size_t i = 0; i < ARRAYSIZE(rates); i++)
	{
		if (rates[i] == rate)
			return WINPR_ASSERTING_INT_CAST(int, i);
	}
	return -1;
}

WINPR_ATTR_NODISCARD
static BOOL aac_build_asc(UINT32 rate, UINT16 channels, BYTE asc[2])
{
	WINPR_ASSERT(asc);

	const int sfIndex = aac_sample_rate_index(rate);
	if (sfIndex < 0)
		return FALSE;

	if ((channels < 1) || (channels > 8))
		return FALSE;

	/* channelConfiguration 8 is reserved, 7.1 is signalled as 7 */
	const int channelConfig = (channels == 8) ? 7 : channels;

	asc[0] = WINPR_ASSERTING_INT_CAST(BYTE, (AAC_OBJECT_LC << 3) | ((sfIndex >> 1) & 0x07));
	asc[1] =
	    WINPR_ASSERTING_INT_CAST(BYTE, ((sfIndex & 0x01) << 7) | ((channelConfig & 0x0F) << 3));
	return TRUE;
}

/* Reject extra format bytes that are not an AudioSpecificConfig. */
WINPR_ATTR_NODISCARD
static BOOL aac_asc_plausible(const BYTE* data, size_t size)
{
	WINPR_ASSERT(data);

	if (size < 2)
		return FALSE;

	const int objectType = (data[0] >> 3) & 0x1F;
	const int sfIndex = ((data[0] & 0x07) << 1) | ((data[1] >> 7) & 0x01);

	switch (objectType)
	{
		case 1:  /* AAC Main */
		case 2:  /* AAC-LC */
		case 3:  /* AAC-SSR */
		case 4:  /* AAC-LTP */
		case 5:  /* SBR */
		case 29: /* PS */
			break;
		default:
			return FALSE;
	}

	/* Explicit SBR/PS signalling appends an extension sampling frequency index plus the base
	 * object type and config, so a two byte buffer claiming either is truncated. */
	if (((objectType == 5) || (objectType == 29)) && (size < 4))
		return FALSE;

	if (sfIndex == 15)
		return size >= 5; /* explicit 24 bit sampling frequency follows the index */

	if (sfIndex > 12)
		return FALSE;

	/* channelConfiguration 8..15 is reserved; 0 defers the layout to the specific config */
	return (((data[1] >> 3) & 0x0F) <= 7);
}

/* The format extra bytes may hold a bare AudioSpecificConfig or a HEAACWAVEINFO header
 * (wPayloadType, wAudioProfileLevelIndication, wStructType, wReserved1, dwReserved2) in front
 * of one. Anything else is not a config and must not reach the decoder. */
WINPR_ATTR_NODISCARD
static BOOL aac_format_asc(const AUDIO_FORMAT* format, const BYTE** pasc, size_t* psize)
{
	WINPR_ASSERT(format);
	WINPR_ASSERT(pasc);
	WINPR_ASSERT(psize);

	const size_t heaacInfoSize = 12;

	if (!format->data || (format->cbSize < 2))
		return FALSE;

	if (aac_asc_plausible(format->data, format->cbSize))
	{
		*pasc = format->data;
		*psize = format->cbSize;
		return TRUE;
	}

	if ((format->cbSize >= heaacInfoSize + 2) &&
	    aac_asc_plausible(&format->data[heaacInfoSize], format->cbSize - heaacInfoSize))
	{
		*pasc = &format->data[heaacInfoSize];
		*psize = format->cbSize - heaacInfoSize;
		return TRUE;
	}

	return FALSE;
}

WINPR_ATTR_NODISCARD
static BOOL mediacodec_drain_output(MEDIACODEC_AAC* mc, wStream* out)
{
	WINPR_ASSERT(mc);
	WINPR_ASSERT(out);

	/* Wait for the codec on the first attempt only: once it has produced output the remaining
	 * buffers are already queued, and paying the timeout again just stalls the channel. */
	int64_t timeout = MEDIACODEC_DSP_DEQUEUE_TIMEOUT;

	for (int loops = 0; loops <= MEDIACODEC_DSP_MAX_RETRIES; loops++)
	{
		AMediaCodecBufferInfo info = { 0 };
		const ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(mc->codec, &info, timeout);
		if (outIdx >= 0)
		{
			timeout = 0;

			if (info.size > 0 && (info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) == 0)
			{
				size_t outCap = 0;
				uint8_t* outBuf = AMediaCodec_getOutputBuffer(mc->codec, (size_t)outIdx, &outCap);

				/* offset and size come from the driver, they are not trusted bounds */
				if (outBuf && ((info.offset < 0) || ((size_t)info.offset > outCap) ||
				               ((size_t)info.size > outCap - (size_t)info.offset)))
				{
					WLog_ERR(TAG, "output buffer info out of range: offset %d size %d cap %zu",
					         info.offset, info.size, outCap);
					mediacodec_aac_warn(
					    AMediaCodec_releaseOutputBuffer(mc->codec, (size_t)outIdx, FALSE),
					    "releaseOutputBuffer");
					return FALSE;
				}

				if (outBuf)
				{
					if (!Stream_EnsureRemainingCapacity(out, (size_t)info.size))
					{
						mediacodec_aac_warn(
						    AMediaCodec_releaseOutputBuffer(mc->codec, (size_t)outIdx, FALSE),
						    "releaseOutputBuffer");
						return FALSE;
					}
					Stream_Write(out, outBuf + info.offset, (size_t)info.size);
				}
			}
			const media_status_t rc =
			    AMediaCodec_releaseOutputBuffer(mc->codec, (size_t)outIdx, FALSE);
			if (rc != AMEDIA_OK)
			{
				WLog_ERR(TAG, "releaseOutputBuffer failed: %d", rc);
				return FALSE;
			}
		}
		else if (outIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
		{
			AMediaFormat* newFormat = AMediaCodec_getOutputFormat(mc->codec);
			if (newFormat)
			{
				int32_t newRate = 0;
				int32_t newCh = 0;
				if (AMediaFormat_getInt32(newFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, &newRate) &&
				    (newRate > 0))
					mc->outSampleRate = (UINT32)newRate;
				if (AMediaFormat_getInt32(newFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &newCh) &&
				    (newCh > 0) && (newCh <= UINT16_MAX))
					mc->outChannels = WINPR_ASSERTING_INT_CAST(UINT16, newCh);
				mediacodec_aac_warn(AMediaFormat_delete(newFormat), "AMediaFormat_delete");

				WLog_DBG(TAG, "output format %" PRIu32 " Hz, %" PRIu16 " ch", mc->outSampleRate,
				         mc->outChannels);

				/* The caller opened its device for the negotiated format and there is no
				 * resampler here, so a codec decoding to something else (SBR doubles the rate)
				 * plays back detuned. Report it instead of failing the channel. */
				if (!mc->formatMismatchLogged &&
				    ((mc->outSampleRate != mc->sampleRate) || (mc->outChannels != mc->channels)))
				{
					mc->formatMismatchLogged = TRUE;
					WLog_WARN(TAG,
					          "decoder output %" PRIu32 " Hz/%" PRIu16 " ch differs from the "
					          "negotiated %" PRIu32 " Hz/%" PRIu16 " ch, playback will be wrong",
					          mc->outSampleRate, mc->outChannels, mc->sampleRate, mc->channels);
				}
			}
		}
		else if (outIdx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
		{
			continue;
		}
		else if (outIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
		{
			break;
		}
		else
		{
			WLog_ERR(TAG, "dequeueOutputBuffer returned %zd", outIdx);
			return FALSE;
		}
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL mediacodec_feed_buffer(MEDIACODEC_AAC* mc, const BYTE* data, size_t length,
                                   size_t samplesQueued, wStream* out)
{
	WINPR_ASSERT(mc);
	WINPR_ASSERT(data);
	WINPR_ASSERT(out);

	int retries = 0;
	ssize_t inIdx = -1;
	while ((inIdx = AMediaCodec_dequeueInputBuffer(mc->codec, MEDIACODEC_DSP_DEQUEUE_TIMEOUT)) < 0)
	{
		if (inIdx != AMEDIACODEC_INFO_TRY_AGAIN_LATER)
		{
			WLog_ERR(TAG, "dequeueInputBuffer failed [%zd]", inIdx);
			return FALSE;
		}

		if (++retries > MEDIACODEC_DSP_MAX_RETRIES)
		{
			WLog_ERR(TAG, "dequeueInputBuffer timeout [%zd]", inIdx);
			return FALSE;
		}
	}

	size_t inCap = 0;
	uint8_t* inBuf = AMediaCodec_getInputBuffer(mc->codec, (size_t)inIdx, &inCap);
	if (!inBuf || inCap < length)
	{
		WLog_ERR(TAG, "input buffer too small: inCap %zu < length %zu", inCap, length);
		mediacodec_aac_warn(AMediaCodec_queueInputBuffer(mc->codec, (size_t)inIdx, 0, 0, 0, 0),
		                    "queueInputBuffer");
		return FALSE;
	}

	memcpy(inBuf, data, length);

	/* Monotonically increasing PTS in microseconds */
	uint64_t ptsUs = 0;
	if (mc->sampleRate > 0)
		ptsUs = (mc->sampleCount / mc->sampleRate) * 1000000ULL +
		        (mc->sampleCount % mc->sampleRate) * 1000000ULL / mc->sampleRate;
	mc->sampleCount += samplesQueued;

	if (AMediaCodec_queueInputBuffer(mc->codec, (size_t)inIdx, 0, length, ptsUs, 0) != AMEDIA_OK)
	{
		WLog_ERR(TAG, "queueInputBuffer failed");
		return FALSE;
	}

	return mediacodec_drain_output(mc, out);
}

BOOL mediacodec_aac_dsp_init(FREERDP_DSP_COMMON_CONTEXT* context, size_t frames_per_packet)
{
	WINPR_ASSERT(context);
	WINPR_UNUSED(frames_per_packet);

	MEDIACODEC_AAC* mc = (MEDIACODEC_AAC*)calloc(1, sizeof(MEDIACODEC_AAC));
	if (!mc)
		return FALSE;
	context->mediacodecAacInstance = mc;

	if ((context->format.nSamplesPerSec == 0) ||
	    (context->format.nSamplesPerSec > AAC_MAX_SAMPLE_RATE) || (context->format.nChannels < 1) ||
	    (context->format.nChannels > 8))
	{
		WLog_ERR(TAG, "unusable AAC format %" PRIu32 " Hz, %" PRIu16 " ch",
		         context->format.nSamplesPerSec, context->format.nChannels);
		free(mc);
		context->mediacodecAacInstance = nullptr;
		return FALSE;
	}

	mc->isEncoder = context->encoder;
	mc->sampleRate = context->format.nSamplesPerSec;
	mc->channels = context->format.nChannels;
	mc->outSampleRate = mc->sampleRate;
	mc->outChannels = mc->channels;
	mc->sampleCount = 0;

	if (mc->isEncoder)
	{
		mc->encFrameBytes = AAC_FRAME_SAMPLES * mc->channels * sizeof(int16_t);
		mc->encBuffer = (BYTE*)calloc(mc->encFrameBytes, sizeof(BYTE));
		if (!mc->encBuffer)
		{
			free(mc);
			context->mediacodecAacInstance = nullptr;
			return FALSE;
		}
		mc->encBufferedBytes = 0;
	}

	mc->codec = context->encoder ? AMediaCodec_createEncoderByType(AAC_MIME)
	                             : AMediaCodec_createDecoderByType(AAC_MIME);
	if (!mc->codec)
	{
		WLog_ERR(TAG, "AMediaCodec_create%scoderByType failed", context->encoder ? "En" : "De");
		free(mc->encBuffer);
		free(mc);
		context->mediacodecAacInstance = nullptr;
		return FALSE;
	}

	AMediaFormat* format = AMediaFormat_new();
	if (!format)
	{
		mediacodec_aac_dsp_uninit(context);
		return FALSE;
	}

	AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, AAC_MIME);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE,
	                      WINPR_ASSERTING_INT_CAST(int32_t, mc->sampleRate));
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT,
	                      WINPR_ASSERTING_INT_CAST(int32_t, mc->channels));

	if (context->encoder)
	{
		const UINT64 bps = (UINT64)context->format.nAvgBytesPerSec * 8;
		int32_t bitrate = 128000;
		if (bps > INT32_MAX)
			bitrate = INT32_MAX;
		else if (bps > 0)
			bitrate = WINPR_ASSERTING_INT_CAST(int32_t, bps);
		AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
		AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_AAC_PROFILE, AAC_OBJECT_LC);
	}
	else
	{
		/* Allow large compressed inputs to prevent buffer truncation */
		AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, DECODER_MAX_INPUT_SIZE);

		const BYTE* asc = nullptr;
		size_t ascSize = 0;
		BYTE built[2];

		if (!aac_format_asc(&context->format, &asc, &ascSize))
		{
			if (!aac_build_asc(mc->sampleRate, mc->channels, built))
			{
				WLog_ERR(TAG, "unsupported AAC sample rate %" PRIu32, mc->sampleRate);
				mediacodec_aac_warn(AMediaFormat_delete(format), "AMediaFormat_delete");
				mediacodec_aac_dsp_uninit(context);
				return FALSE;
			}

			asc = built;
			ascSize = sizeof(built);
		}

		AMediaFormat_setBuffer(format, MEDIACODEC_KEY_CSD_0, asc, ascSize);
	}

	media_status_t status =
	    AMediaCodec_configure(mc->codec, format, nullptr, nullptr,
	                          context->encoder ? AMEDIACODEC_CONFIGURE_FLAG_ENCODE : 0);
	mediacodec_aac_warn(AMediaFormat_delete(format), "AMediaFormat_delete");
	if (status != AMEDIA_OK)
	{
		WLog_ERR(TAG, "AMediaCodec_configure failed: %d", status);
		mediacodec_aac_dsp_uninit(context);
		return FALSE;
	}

	status = AMediaCodec_start(mc->codec);
	if (status != AMEDIA_OK)
	{
		WLog_ERR(TAG, "AMediaCodec_start failed: %d", status);
		mediacodec_aac_dsp_uninit(context);
		return FALSE;
	}

	mc->started = TRUE;
	WLog_DBG(TAG, "MediaCodec AAC %scoder started: %" PRIu32 " Hz, %" PRIu16 " ch",
	         context->encoder ? "en" : "de", mc->sampleRate, mc->channels);
	return TRUE;
}

void mediacodec_aac_dsp_uninit(FREERDP_DSP_COMMON_CONTEXT* context)
{
	if (!context)
		return;

	MEDIACODEC_AAC* mc = (MEDIACODEC_AAC*)context->mediacodecAacInstance;
	if (!mc)
		return;

	if (mc->codec)
	{
		if (mc->started)
			mediacodec_aac_warn(AMediaCodec_stop(mc->codec), "AMediaCodec_stop");
		mediacodec_aac_warn(AMediaCodec_delete(mc->codec), "AMediaCodec_delete");
	}
	free(mc->encBuffer);
	free(mc);
	context->mediacodecAacInstance = nullptr;
}

BOOL mediacodec_aac_dsp_encode(FREERDP_DSP_COMMON_CONTEXT* context, const AUDIO_FORMAT* srcFormat,
                               const BYTE* data, size_t length, wStream* out)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(srcFormat);

	if (srcFormat->wFormatTag != WAVE_FORMAT_PCM)
	{
		WLog_WARN(TAG, "expected PCM input, got 0x%04x", (unsigned)srcFormat->wFormatTag);
		return FALSE;
	}

	MEDIACODEC_AAC* mc = (MEDIACODEC_AAC*)context->mediacodecAacInstance;
	if (!mc || !mc->codec || !mc->started || !data || length == 0 || !out)
		return FALSE;

	if (srcFormat->wBitsPerSample != 16)
	{
		WLog_ERR(TAG, "expected 16 bit PCM, got %" PRIu16 " bits", srcFormat->wBitsPerSample);
		return FALSE;
	}

	const size_t srcFrameBytes = srcFormat->nChannels * sizeof(int16_t);
	if ((srcFrameBytes == 0) || ((length % srcFrameBytes) != 0))
	{
		WLog_ERR(TAG, "PCM length %" PRIuz " is not a multiple of the %" PRIuz " byte frame",
		         length, srcFrameBytes);
		return FALSE;
	}

	/* There is no resampler or channel mixer here and the dispatcher routes AAC straight to
	 * this backend, so a capture format that drifted from the negotiated one is only
	 * detectable at this point. */
	if (!mc->srcMismatchLogged &&
	    ((srcFormat->nSamplesPerSec != mc->sampleRate) || (srcFormat->nChannels != mc->channels)))
	{
		mc->srcMismatchLogged = TRUE;
		WLog_WARN(TAG,
		          "PCM source %" PRIu32 " Hz/%" PRIu16 " ch differs from the negotiated %" PRIu32
		          " Hz/%" PRIu16 " ch, encoded audio will be wrong",
		          srcFormat->nSamplesPerSec, srcFormat->nChannels, mc->sampleRate, mc->channels);
	}

	const BYTE* inPtr = data;
	size_t inRemaining = length;

	while (inRemaining > 0)
	{
		/* Fill the accumulation buffer up to 1024 frames */
		const size_t needed = mc->encFrameBytes - mc->encBufferedBytes;
		const size_t toCopy = (inRemaining < needed) ? inRemaining : needed;

		memcpy(mc->encBuffer + mc->encBufferedBytes, inPtr, toCopy);
		mc->encBufferedBytes += toCopy;
		inPtr += toCopy;
		inRemaining -= toCopy;

		if (mc->encBufferedBytes == mc->encFrameBytes)
		{
			/* Exactly 1024 frames ready: queue to MediaCodec. Drop the frame on failure, a
			 * retained accumulator would be re-fed and duplicate audio. */
			const BOOL fed = mediacodec_feed_buffer(mc, mc->encBuffer, mc->encFrameBytes,
			                                        AAC_FRAME_SAMPLES, out);
			mc->encBufferedBytes = 0;
			if (!fed)
				return FALSE;
		}
	}

	return TRUE;
}

BOOL mediacodec_aac_dsp_decode(FREERDP_DSP_COMMON_CONTEXT* context, const AUDIO_FORMAT* srcFormat,
                               const BYTE* data, size_t length, wStream* out)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(srcFormat);

	if (srcFormat->wFormatTag != WAVE_FORMAT_AAC_MS)
	{
		WLog_WARN(TAG, "expected AAC input, got 0x%04x", (unsigned)srcFormat->wFormatTag);
		return FALSE;
	}

	MEDIACODEC_AAC* mc = (MEDIACODEC_AAC*)context->mediacodecAacInstance;
	if (!mc || !mc->codec || !mc->started || !data || length == 0 || !out)
		return FALSE;

	/* In AAC decode, one AU is typically 1024 output frames */
	return mediacodec_feed_buffer(mc, data, length, AAC_FRAME_SAMPLES, out);
}

BOOL mediacodec_aac_dsp_supports_format(const AUDIO_FORMAT* format, BOOL encode)
{
	WINPR_ASSERT(format);

	if (format->wFormatTag != WAVE_FORMAT_AAC_MS)
		return FALSE;

	if ((format->nSamplesPerSec == 0) || (format->nSamplesPerSec > AAC_MAX_SAMPLE_RATE) ||
	    (format->nChannels < 1) || (format->nChannels > 8))
		return FALSE;

	if (encode && (format->nChannels > 2))
		return FALSE;

	if (!encode)
	{
		const BYTE* asc = nullptr;
		size_t ascSize = 0;

		if (aac_format_asc(format, &asc, &ascSize))
			return TRUE;
	}

	/* Without a sampling frequency index there is no AudioSpecificConfig to configure with,
	 * and advertising the format would only fail later when the device is opened. */
	return aac_sample_rate_index(format->nSamplesPerSec) >= 0;
}
