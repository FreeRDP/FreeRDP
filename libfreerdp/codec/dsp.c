/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Digital Sound Processing
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

#include <freerdp/config.h>

#include <winpr/assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/types.h>
#include <freerdp/log.h>
#include <freerdp/codec/dsp.h>

#include "dsp.h"

#if defined(WITH_FDK_AAC)
#include "dsp_fdk_aac.h"
#endif

#if !defined(WITH_DSP_FFMPEG)
#if defined(WITH_GSM)
#include <gsm/gsm.h>
#endif

#if defined(WITH_LAME)
#include <lame/lame.h>
#endif

#if defined(WITH_OPUS)
#include <opus/opus.h>

#define OPUS_MAX_FRAMES 5760
#endif

#if defined(WITH_FAAD2)
#include <neaacdec.h>
#endif

#if defined(WITH_FAAC)
#include <faac.h>
#endif

#if defined(WITH_SOXR)
#include <soxr.h>
#endif

#else
#include "dsp_ffmpeg.h"
#endif

#if !defined(WITH_DSP_FFMPEG)

#define TAG FREERDP_TAG("dsp")

typedef union
{
	struct
	{
		size_t packet_size;
		INT16 last_sample[2];
		INT16 last_step[2];
	} ima;
	struct
	{
		BYTE predictor[2];
		INT32 delta[2];
		INT32 sample1[2];
		INT32 sample2[2];
	} ms;
} ADPCM;

struct S_FREERDP_DSP_CONTEXT
{
	FREERDP_DSP_COMMON_CONTEXT common;

	ADPCM adpcm;

#if defined(WITH_GSM)
	gsm gsm;
#endif
#if defined(WITH_LAME)
	lame_t lame;
	hip_t hip;
#endif
#if defined(WITH_OPUS)
	OpusDecoder* opus_decoder;
	OpusEncoder* opus_encoder;
#endif
#if defined(WITH_FAAD2)
	NeAACDecHandle faad;
	BOOL faadSetup;
#endif

#if defined(WITH_FAAC)
	faacEncHandle faac;
	unsigned long faacInputSamples;
	unsigned long faacMaxOutputBytes;
#endif

#if defined(WITH_SOXR)
	soxr_t sox;
#endif
};

#if defined(WITH_OPUS)
static BOOL opus_is_valid_samplerate(const AUDIO_FORMAT* WINPR_RESTRICT format)
{
	WINPR_ASSERT(format);

	switch (format->nSamplesPerSec)
	{
		case 8000:
		case 12000:
		case 16000:
		case 24000:
		case 48000:
			return TRUE;
		default:
			return FALSE;
	}
}
#endif

static INT16 read_int16(const BYTE* WINPR_RESTRICT src)
{
	return (INT16)(src[0] | (src[1] << 8));
}

static BOOL freerdp_dsp_channel_mix(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                    const BYTE* WINPR_RESTRICT src, size_t size,
                                    const AUDIO_FORMAT* WINPR_RESTRICT srcFormat,
                                    const BYTE** WINPR_RESTRICT data, size_t* WINPR_RESTRICT length)
{
	if (!context || !data || !length)
		return FALSE;

	if (srcFormat->wFormatTag != WAVE_FORMAT_PCM)
		return FALSE;

	const UINT32 bpp = srcFormat->wBitsPerSample > 8 ? 2 : 1;
	const size_t samples = size / bpp / srcFormat->nChannels;

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
				return TRUE;

			case 2:  /* We only support stereo, so we can not handle this case. */
			default: /* Unsupported number of channels */
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
			return TRUE;

		case 1:  /* Invalid, do we want to use a 0 channel sound? */
		default: /* Unsupported number of channels */
			return FALSE;
	}

	return FALSE;
}

/**
 * Microsoft Multimedia Standards Update
 * http://download.microsoft.com/download/9/8/6/9863C72A-A3AA-4DDB-B1BA-CA8D17EFD2D4/RIFFNEW.pdf
 */

static BOOL freerdp_dsp_resample(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                 const BYTE* WINPR_RESTRICT src, size_t size,
                                 const AUDIO_FORMAT* WINPR_RESTRICT srcFormat,
                                 const BYTE** WINPR_RESTRICT data, size_t* WINPR_RESTRICT length)
{
#if defined(WITH_SOXR)
	soxr_error_t error;
	size_t idone, odone;
	size_t sframes, rframes;
	size_t rsize;
	size_t sbytes, rbytes;
	size_t dstChannels;
	size_t srcChannels;
	size_t srcBytesPerFrame, dstBytesPerFrame;
#endif
	AUDIO_FORMAT format;

	if (srcFormat->wFormatTag != WAVE_FORMAT_PCM)
	{
		WLog_ERR(TAG, "requires %s for sample input, got %s",
		         audio_format_get_tag_string(WAVE_FORMAT_PCM),
		         audio_format_get_tag_string(srcFormat->wFormatTag));
		return FALSE;
	}

	/* We want to ignore differences of source and destination format. */
	format = *srcFormat;
	format.wFormatTag = WAVE_FORMAT_UNKNOWN;
	format.wBitsPerSample = 0;

	if (audio_format_compatible(&format, &context->common.format))
	{
		*data = src;
		*length = size;
		return TRUE;
	}

#if defined(WITH_SOXR)
	srcBytesPerFrame = (srcFormat->wBitsPerSample > 8) ? 2 : 1;
	dstBytesPerFrame = (context->common.format.wBitsPerSample > 8) ? 2 : 1;
	srcChannels = srcFormat->nChannels;
	dstChannels = context->common.format.nChannels;
	sbytes = srcChannels * srcBytesPerFrame;
	sframes = size / sbytes;
	rbytes = dstBytesPerFrame * dstChannels;
	/* Integer rounding correct division */
	rframes =
	    (sframes * context->common.format.nSamplesPerSec + (srcFormat->nSamplesPerSec + 1) / 2) /
	    srcFormat->nSamplesPerSec;
	rsize = rframes * rbytes;

	if (!Stream_EnsureCapacity(context->common.resample, rsize))
		return FALSE;

	error =
	    soxr_process(context->sox, src, sframes, &idone, Stream_Buffer(context->common.resample),
	                 Stream_Capacity(context->common.resample) / rbytes, &odone);
	Stream_SetLength(context->common.resample, odone * rbytes);
	*data = Stream_Buffer(context->common.resample);
	*length = Stream_Length(context->common.resample);
	return (error == 0) ? TRUE : FALSE;
#else
	WLog_ERR(TAG, "Missing resample support, recompile -DWITH_SOXR=ON or -DWITH_DSP_FFMPEG=ON");
	return FALSE;
#endif
}

/**
 * Microsoft IMA ADPCM specification:
 *
 * http://wiki.multimedia.cx/index.php?title=Microsoft_IMA_ADPCM
 * http://wiki.multimedia.cx/index.php?title=IMA_ADPCM
 */

static const INT16 ima_step_index_table[] = {
	-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8
};

static const INT16 ima_step_size_table[] = {
	7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,   21,    23,
	25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,   73,    80,
	88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,  253,   279,
	307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,  876,   963,
	1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749, 3024,  3327,
	3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487,
	12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static UINT16 dsp_decode_ima_adpcm_sample(ADPCM* WINPR_RESTRICT adpcm, unsigned int channel,
                                          BYTE sample)
{
	const INT32 ss = ima_step_size_table[adpcm->ima.last_step[channel]];
	INT32 d = (ss >> 3);

	if (sample & 1)
		d += (ss >> 2);

	if (sample & 2)
		d += (ss >> 1);

	if (sample & 4)
		d += ss;

	if (sample & 8)
		d = -d;

	d += adpcm->ima.last_sample[channel];

	if (d < -32768)
		d = -32768;
	else if (d > 32767)
		d = 32767;

	adpcm->ima.last_sample[channel] = (INT16)d;
	adpcm->ima.last_step[channel] += ima_step_index_table[sample];

	if (adpcm->ima.last_step[channel] < 0)
		adpcm->ima.last_step[channel] = 0;
	else if (adpcm->ima.last_step[channel] > 88)
		adpcm->ima.last_step[channel] = 88;

	return (UINT16)d;
}

static BOOL freerdp_dsp_decode_ima_adpcm(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                         const BYTE* WINPR_RESTRICT src, size_t size,
                                         wStream* WINPR_RESTRICT out)
{
	size_t out_size = size * 4;
	const UINT32 block_size = context->common.format.nBlockAlign;
	const UINT32 channels = context->common.format.nChannels;

	if (!Stream_EnsureCapacity(out, out_size))
		return FALSE;

	while (size > 0)
	{
		if (size % block_size == 0)
		{
			context->adpcm.ima.last_sample[0] =
			    (INT16)(((UINT16)(*src)) | (((UINT16)(*(src + 1))) << 8));
			context->adpcm.ima.last_step[0] = (INT16)(*(src + 2));
			src += 4;
			size -= 4;
			out_size -= 16;

			if (channels > 1)
			{
				context->adpcm.ima.last_sample[1] =
				    (INT16)(((UINT16)(*src)) | (((UINT16)(*(src + 1))) << 8));
				context->adpcm.ima.last_step[1] = (INT16)(*(src + 2));
				src += 4;
				size -= 4;
				out_size -= 16;
			}
		}

		if (channels > 1)
		{
			for (size_t i = 0; i < 8; i++)
			{
				BYTE* dst = Stream_Pointer(out);

				const int channel = (i < 4 ? 0 : 1);
				{
					const BYTE sample = ((*src) & 0x0f);
					const UINT16 decoded =
					    dsp_decode_ima_adpcm_sample(&context->adpcm, channel, sample);
					dst[((i & 3) << 3) + (channel << 1)] = (decoded & 0xFF);
					dst[((i & 3) << 3) + (channel << 1) + 1] = (decoded >> 8);
				}
				{
					const BYTE sample = ((*src) >> 4);
					const UINT16 decoded =
					    dsp_decode_ima_adpcm_sample(&context->adpcm, channel, sample);
					dst[((i & 3) << 3) + (channel << 1) + 4] = (decoded & 0xFF);
					dst[((i & 3) << 3) + (channel << 1) + 5] = (decoded >> 8);
				}
				src++;
			}

			if (!Stream_SafeSeek(out, 32))
				return FALSE;
			size -= 8;
		}
		else
		{
			BYTE* dst = Stream_Pointer(out);
			if (!Stream_SafeSeek(out, 4))
				return FALSE;

			{
				const BYTE sample = ((*src) & 0x0f);
				const UINT16 decoded = dsp_decode_ima_adpcm_sample(&context->adpcm, 0, sample);
				*dst++ = (decoded & 0xFF);
				*dst++ = (decoded >> 8);
			}
			{
				const BYTE sample = ((*src) >> 4);
				const UINT16 decoded = dsp_decode_ima_adpcm_sample(&context->adpcm, 0, sample);
				*dst++ = (decoded & 0xFF);
				*dst++ = (decoded >> 8);
			}
			src++;
			size--;
		}
	}

	return TRUE;
}

#if defined(WITH_GSM)
static BOOL freerdp_dsp_decode_gsm610(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                      const BYTE* WINPR_RESTRICT src, size_t size,
                                      wStream* WINPR_RESTRICT out)
{
	size_t offset = 0;

	while (offset < size)
	{
		int rc;
		gsm_signal gsmBlockBuffer[160] = { 0 };
		rc = gsm_decode(context->gsm, (gsm_byte*)/* API does not modify */ &src[offset],
		                gsmBlockBuffer);

		if (rc < 0)
			return FALSE;

		if ((offset % 65) == 0)
			offset += 33;
		else
			offset += 32;

		if (!Stream_EnsureRemainingCapacity(out, sizeof(gsmBlockBuffer)))
			return FALSE;

		Stream_Write(out, (void*)gsmBlockBuffer, sizeof(gsmBlockBuffer));
	}

	return TRUE;
}

static BOOL freerdp_dsp_encode_gsm610(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                      const BYTE* WINPR_RESTRICT src, size_t size,
                                      wStream* WINPR_RESTRICT out)
{
	size_t offset = 0;

	while (offset < size)
	{
		const gsm_signal* signal = (const gsm_signal*)&src[offset];

		if (!Stream_EnsureRemainingCapacity(out, sizeof(gsm_frame)))
			return FALSE;

		gsm_encode(context->gsm, (gsm_signal*)/* API does not modify */ signal,
		           Stream_Pointer(out));

		if ((offset % 65) == 0)
			Stream_Seek(out, 33);
		else
			Stream_Seek(out, 32);

		offset += 160;
	}

	return TRUE;
}
#endif

#if defined(WITH_LAME)
static BOOL freerdp_dsp_decode_mp3(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                   const BYTE* WINPR_RESTRICT src, size_t size,
                                   wStream* WINPR_RESTRICT out)
{
	int rc;
	short* pcm_l;
	short* pcm_r;
	size_t buffer_size;

	if (!context || !src || !out)
		return FALSE;

	buffer_size = 2 * context->common.format.nChannels * context->common.format.nSamplesPerSec;

	if (!Stream_EnsureCapacity(context->common.buffer, 2 * buffer_size))
		return FALSE;

	pcm_l = Stream_BufferAs(context->common.buffer, short);
	pcm_r = Stream_BufferAs(context->common.buffer, short) + buffer_size;
	rc = hip_decode(context->hip, (unsigned char*)/* API is not modifying content */ src, size,
	                pcm_l, pcm_r);

	if (rc <= 0)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(out, (size_t)rc * context->common.format.nChannels * 2))
		return FALSE;

	for (size_t x = 0; x < rc; x++)
	{
		Stream_Write_UINT16(out, (UINT16)pcm_l[x]);
		Stream_Write_UINT16(out, (UINT16)pcm_r[x]);
	}

	return TRUE;
}

static BOOL freerdp_dsp_encode_mp3(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                   const BYTE* WINPR_RESTRICT src, size_t size,
                                   wStream* WINPR_RESTRICT out)
{
	size_t samples_per_channel;
	int rc;

	if (!context || !src || !out)
		return FALSE;

	samples_per_channel =
	    size / context->common.format.nChannels / context->common.format.wBitsPerSample / 8;

	/* Ensure worst case buffer size for mp3 stream taken from LAME header */
	if (!Stream_EnsureRemainingCapacity(out, 5 / 4 * samples_per_channel + 7200))
		return FALSE;

	samples_per_channel = size / 2 /* size of a sample */ / context->common.format.nChannels;
	rc = lame_encode_buffer_interleaved(context->lame, (short*)src, samples_per_channel,
	                                    Stream_Pointer(out), Stream_GetRemainingCapacity(out));

	if (rc < 0)
		return FALSE;

	Stream_Seek(out, (size_t)rc);
	return TRUE;
}
#endif

#if defined(WITH_FAAC)
static BOOL freerdp_dsp_encode_faac(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                    const BYTE* WINPR_RESTRICT src, size_t size,
                                    wStream* WINPR_RESTRICT out)
{
	const int16_t* inSamples = (const int16_t*)src;
	unsigned int bpp;
	size_t nrSamples;
	int rc;

	if (!context || !src || !out)
		return FALSE;

	bpp = context->common.format.wBitsPerSample / 8;
	nrSamples = size / bpp;

	if (!Stream_EnsureRemainingCapacity(context->common.buffer, nrSamples * sizeof(int16_t)))
		return FALSE;

	for (size_t x = 0; x < nrSamples; x++)
	{
		Stream_Write_INT16(context->common.buffer, inSamples[x]);
		if (Stream_GetPosition(context->common.buffer) / bpp >= context->faacInputSamples)
		{
			if (!Stream_EnsureRemainingCapacity(out, context->faacMaxOutputBytes))
				return FALSE;
			rc = faacEncEncode(context->faac, Stream_BufferAs(context->common.buffer, int32_t),
			                   context->faacInputSamples, Stream_Pointer(out),
			                   Stream_GetRemainingCapacity(out));
			if (rc < 0)
				return FALSE;
			if (rc > 0)
				Stream_Seek(out, (size_t)rc);
			Stream_SetPosition(context->common.buffer, 0);
		}
	}

	return TRUE;
}
#endif

#if defined(WITH_OPUS)
static BOOL freerdp_dsp_decode_opus(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                    const BYTE* WINPR_RESTRICT src, size_t size,
                                    wStream* WINPR_RESTRICT out)
{
	size_t max_size = 5760;
	int frames;

	if (!context || !src || !out)
		return FALSE;

	/* Max packet duration is 120ms (5760 at 48KHz) */
	max_size = OPUS_MAX_FRAMES * context->common.format.nChannels * sizeof(int16_t);
	if (!Stream_EnsureRemainingCapacity(context->common.buffer, max_size))
		return FALSE;

	frames = opus_decode(context->opus_decoder, src, size, Stream_Pointer(out), OPUS_MAX_FRAMES, 0);
	if (frames < 0)
		return FALSE;

	Stream_Seek(out, frames * context->common.format.nChannels * sizeof(int16_t));

	return TRUE;
}

static BOOL freerdp_dsp_encode_opus(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                    const BYTE* WINPR_RESTRICT src, size_t size,
                                    wStream* WINPR_RESTRICT out)
{
	if (!context || !src || !out)
		return FALSE;

	/* Max packet duration is 120ms (5760 at 48KHz) */
	const size_t max_size = OPUS_MAX_FRAMES * context->common.format.nChannels * sizeof(int16_t);
	if (!Stream_EnsureRemainingCapacity(context->common.buffer, max_size))
		return FALSE;

	const int src_frames = size / sizeof(opus_int16) / context->common.format.nChannels;
	const opus_int16* src_data = (const opus_int16*)src;
	const int frames =
	    opus_encode(context->opus_encoder, src_data, src_frames, Stream_Pointer(out), max_size);
	if (frames < 0)
		return FALSE;
	return Stream_SafeSeek(out, frames * context->common.format.nChannels * sizeof(int16_t));
}
#endif

#if defined(WITH_FAAD2)
static BOOL freerdp_dsp_decode_faad(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                    const BYTE* WINPR_RESTRICT src, size_t size,
                                    wStream* WINPR_RESTRICT out)
{
	NeAACDecFrameInfo info;
	size_t offset = 0;

	if (!context || !src || !out)
		return FALSE;

	if (!context->faadSetup)
	{
		union
		{
			const void* cpv;
			void* pv;
		} cnv;
		unsigned long samplerate;
		unsigned char channels;
		long err;
		cnv.cpv = src;
		err = NeAACDecInit(context->faad, /* API is not modifying content */ cnv.pv, size,
		                   &samplerate, &channels);

		if (err != 0)
			return FALSE;

		if (channels != context->common.format.nChannels)
			return FALSE;

		if (samplerate != context->common.format.nSamplesPerSec)
			return FALSE;

		context->faadSetup = TRUE;
	}

	while (offset < size)
	{
		union
		{
			const void* cpv;
			void* pv;
		} cnv;
		size_t outSize;
		void* sample_buffer;
		outSize = context->common.format.nSamplesPerSec * context->common.format.nChannels *
		          context->common.format.wBitsPerSample / 8;

		if (!Stream_EnsureRemainingCapacity(out, outSize))
			return FALSE;

		sample_buffer = Stream_Pointer(out);

		cnv.cpv = &src[offset];
		NeAACDecDecode2(context->faad, &info, cnv.pv, size - offset, &sample_buffer,
		                Stream_GetRemainingCapacity(out));

		if (info.error != 0)
			return FALSE;

		offset += info.bytesconsumed;

		if (info.samples == 0)
			continue;

		Stream_Seek(out, info.samples * context->common.format.wBitsPerSample / 8);
	}

	return TRUE;
}

#endif

/**
 * 0     1     2     3
 * 2 0   6 4   10 8  14 12   <left>
 *
 * 4     5     6     7
 * 3 1   7 5   11 9  15 13   <right>
 */
static const struct
{
	BYTE byte_num;
	BYTE byte_shift;
} ima_stereo_encode_map[] = { { 0, 0 }, { 4, 0 }, { 0, 4 }, { 4, 4 }, { 1, 0 }, { 5, 0 },
	                          { 1, 4 }, { 5, 4 }, { 2, 0 }, { 6, 0 }, { 2, 4 }, { 6, 4 },
	                          { 3, 0 }, { 7, 0 }, { 3, 4 }, { 7, 4 } };

static BYTE dsp_encode_ima_adpcm_sample(ADPCM* WINPR_RESTRICT adpcm, int channel, INT16 sample)
{
	INT32 ss = ima_step_size_table[adpcm->ima.last_step[channel]];
	INT32 e = sample - adpcm->ima.last_sample[channel];
	INT32 d = e;
	INT32 diff = ss >> 3;
	BYTE enc = 0;

	if (e < 0)
	{
		enc = 8;
		e = -e;
	}

	if (e >= ss)
	{
		enc |= 4;
		e -= ss;
	}

	ss >>= 1;

	if (e >= ss)
	{
		enc |= 2;
		e -= ss;
	}

	ss >>= 1;

	if (e >= ss)
	{
		enc |= 1;
		e -= ss;
	}

	if (d < 0)
		diff = d + e - diff;
	else
		diff = d - e + diff;

	diff += adpcm->ima.last_sample[channel];

	if (diff < -32768)
		diff = -32768;
	else if (diff > 32767)
		diff = 32767;

	adpcm->ima.last_sample[channel] = (INT16)diff;
	adpcm->ima.last_step[channel] += ima_step_index_table[enc];

	if (adpcm->ima.last_step[channel] < 0)
		adpcm->ima.last_step[channel] = 0;
	else if (adpcm->ima.last_step[channel] > 88)
		adpcm->ima.last_step[channel] = 88;

	return enc;
}

static BOOL freerdp_dsp_encode_ima_adpcm(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                         const BYTE* WINPR_RESTRICT src, size_t size,
                                         wStream* WINPR_RESTRICT out)
{
	if (!Stream_EnsureRemainingCapacity(out, size))
		return FALSE;
	if (!Stream_EnsureRemainingCapacity(context->common.buffer, size + 64))
		return FALSE;

	const size_t align = (context->common.format.nChannels > 1) ? 32 : 4;

	while (size >= align)
	{
		if (Stream_GetPosition(context->common.buffer) % context->common.format.nBlockAlign == 0)
		{
			Stream_Write_UINT8(context->common.buffer, context->adpcm.ima.last_sample[0] & 0xFF);
			Stream_Write_UINT8(context->common.buffer,
			                   (context->adpcm.ima.last_sample[0] >> 8) & 0xFF);
			Stream_Write_UINT8(context->common.buffer, (BYTE)context->adpcm.ima.last_step[0]);
			Stream_Write_UINT8(context->common.buffer, 0);

			if (context->common.format.nChannels > 1)
			{
				Stream_Write_UINT8(context->common.buffer,
				                   context->adpcm.ima.last_sample[1] & 0xFF);
				Stream_Write_UINT8(context->common.buffer,
				                   (context->adpcm.ima.last_sample[1] >> 8) & 0xFF);
				Stream_Write_UINT8(context->common.buffer, (BYTE)context->adpcm.ima.last_step[1]);
				Stream_Write_UINT8(context->common.buffer, 0);
			}
		}

		if (context->common.format.nChannels > 1)
		{
			BYTE* dst = Stream_Pointer(context->common.buffer);
			ZeroMemory(dst, 8);

			for (size_t i = 0; i < 16; i++)
			{
				const INT16 sample = (INT16)(((UINT16)(*src)) | (((UINT16)(*(src + 1))) << 8));
				src += 2;
				const BYTE encoded = dsp_encode_ima_adpcm_sample(&context->adpcm, i % 2, sample);
				dst[ima_stereo_encode_map[i].byte_num] |= encoded
				                                          << ima_stereo_encode_map[i].byte_shift;
			}

			if (!Stream_SafeSeek(context->common.buffer, 8))
				return FALSE;
			size -= 32;
		}
		else
		{
			INT16 sample = (INT16)(((UINT16)(*src)) | (((UINT16)(*(src + 1))) << 8));
			src += 2;
			BYTE encoded = dsp_encode_ima_adpcm_sample(&context->adpcm, 0, sample);
			sample = (INT16)(((UINT16)(*src)) | (((UINT16)(*(src + 1))) << 8));
			src += 2;
			encoded |= dsp_encode_ima_adpcm_sample(&context->adpcm, 0, sample) << 4;
			Stream_Write_UINT8(context->common.buffer, encoded);
			size -= 4;
		}

		if (Stream_GetPosition(context->common.buffer) >= context->adpcm.ima.packet_size)
		{
			BYTE* bsrc = Stream_Buffer(context->common.buffer);
			Stream_Write(out, bsrc, context->adpcm.ima.packet_size);
			Stream_SetPosition(context->common.buffer, 0);
		}
	}

	return TRUE;
}

/**
 * Microsoft ADPCM Specification:
 *
 * http://wiki.multimedia.cx/index.php?title=Microsoft_ADPCM
 */

static const INT32 ms_adpcm_adaptation_table[] = { 230, 230, 230, 230, 307, 409, 512, 614,
	                                               768, 614, 512, 409, 307, 230, 230, 230 };

static const INT32 ms_adpcm_coeffs1[7] = { 256, 512, 0, 192, 240, 460, 392 };

static const INT32 ms_adpcm_coeffs2[7] = { 0, -256, 0, 64, 0, -208, -232 };

static INLINE INT16 freerdp_dsp_decode_ms_adpcm_sample(ADPCM* WINPR_RESTRICT adpcm, BYTE sample,
                                                       int channel)
{
	const INT8 nibble = (sample & 0x08 ? (INT8)sample - 16 : (INT8)sample);
	INT32 presample =
	    ((adpcm->ms.sample1[channel] * ms_adpcm_coeffs1[adpcm->ms.predictor[channel]]) +
	     (adpcm->ms.sample2[channel] * ms_adpcm_coeffs2[adpcm->ms.predictor[channel]])) /
	    256;
	presample += nibble * adpcm->ms.delta[channel];

	if (presample > 32767)
		presample = 32767;
	else if (presample < -32768)
		presample = -32768;

	adpcm->ms.sample2[channel] = adpcm->ms.sample1[channel];
	adpcm->ms.sample1[channel] = presample;
	adpcm->ms.delta[channel] = adpcm->ms.delta[channel] * ms_adpcm_adaptation_table[sample] / 256;

	if (adpcm->ms.delta[channel] < 16)
		adpcm->ms.delta[channel] = 16;

	return (INT16)presample;
}

static BOOL freerdp_dsp_decode_ms_adpcm(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                        const BYTE* WINPR_RESTRICT src, size_t size,
                                        wStream* WINPR_RESTRICT out)
{
	const size_t out_size = size * 4;
	const UINT32 channels = context->common.format.nChannels;
	const UINT32 block_size = context->common.format.nBlockAlign;

	if (!Stream_EnsureCapacity(out, out_size))
		return FALSE;

	while (size > 0)
	{
		if (size % block_size == 0)
		{
			if (channels > 1)
			{
				context->adpcm.ms.predictor[0] = *src++;
				context->adpcm.ms.predictor[1] = *src++;
				context->adpcm.ms.delta[0] = read_int16(src);
				src += 2;
				context->adpcm.ms.delta[1] = read_int16(src);
				src += 2;
				context->adpcm.ms.sample1[0] = read_int16(src);
				src += 2;
				context->adpcm.ms.sample1[1] = read_int16(src);
				src += 2;
				context->adpcm.ms.sample2[0] = read_int16(src);
				src += 2;
				context->adpcm.ms.sample2[1] = read_int16(src);
				src += 2;
				size -= 14;
				Stream_Write_INT16(out, (INT16)context->adpcm.ms.sample2[0]);
				Stream_Write_INT16(out, (INT16)context->adpcm.ms.sample2[1]);
				Stream_Write_INT16(out, (INT16)context->adpcm.ms.sample1[0]);
				Stream_Write_INT16(out, (INT16)context->adpcm.ms.sample1[1]);
			}
			else
			{
				context->adpcm.ms.predictor[0] = *src++;
				context->adpcm.ms.delta[0] = read_int16(src);
				src += 2;
				context->adpcm.ms.sample1[0] = read_int16(src);
				src += 2;
				context->adpcm.ms.sample2[0] = read_int16(src);
				src += 2;
				size -= 7;
				Stream_Write_INT16(out, (INT16)context->adpcm.ms.sample2[0]);
				Stream_Write_INT16(out, (INT16)context->adpcm.ms.sample1[0]);
			}
		}

		if (channels > 1)
		{
			{
				const BYTE sample = *src++;
				size--;
				Stream_Write_INT16(
				    out, freerdp_dsp_decode_ms_adpcm_sample(&context->adpcm, sample >> 4, 0));
				Stream_Write_INT16(
				    out, freerdp_dsp_decode_ms_adpcm_sample(&context->adpcm, sample & 0x0F, 1));
			}
			{
				const BYTE sample = *src++;
				size--;
				Stream_Write_INT16(
				    out, freerdp_dsp_decode_ms_adpcm_sample(&context->adpcm, sample >> 4, 0));
				Stream_Write_INT16(
				    out, freerdp_dsp_decode_ms_adpcm_sample(&context->adpcm, sample & 0x0F, 1));
			}
		}
		else
		{
			const BYTE sample = *src++;
			size--;
			Stream_Write_INT16(out,
			                   freerdp_dsp_decode_ms_adpcm_sample(&context->adpcm, sample >> 4, 0));
			Stream_Write_INT16(
			    out, freerdp_dsp_decode_ms_adpcm_sample(&context->adpcm, sample & 0x0F, 0));
		}
	}

	return TRUE;
}

static BYTE freerdp_dsp_encode_ms_adpcm_sample(ADPCM* WINPR_RESTRICT adpcm, INT32 sample,
                                               int channel)
{
	INT32 presample =
	    ((adpcm->ms.sample1[channel] * ms_adpcm_coeffs1[adpcm->ms.predictor[channel]]) +
	     (adpcm->ms.sample2[channel] * ms_adpcm_coeffs2[adpcm->ms.predictor[channel]])) /
	    256;
	INT32 errordelta = (sample - presample) / adpcm->ms.delta[channel];

	if ((sample - presample) % adpcm->ms.delta[channel] > adpcm->ms.delta[channel] / 2)
		errordelta++;

	if (errordelta > 7)
		errordelta = 7;
	else if (errordelta < -8)
		errordelta = -8;

	presample += adpcm->ms.delta[channel] * errordelta;

	if (presample > 32767)
		presample = 32767;
	else if (presample < -32768)
		presample = -32768;

	adpcm->ms.sample2[channel] = adpcm->ms.sample1[channel];
	adpcm->ms.sample1[channel] = presample;
	adpcm->ms.delta[channel] =
	    adpcm->ms.delta[channel] * ms_adpcm_adaptation_table[(((BYTE)errordelta) & 0x0F)] / 256;

	if (adpcm->ms.delta[channel] < 16)
		adpcm->ms.delta[channel] = 16;

	return ((BYTE)errordelta) & 0x0F;
}

static BOOL freerdp_dsp_encode_ms_adpcm(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                                        const BYTE* WINPR_RESTRICT src, size_t size,
                                        wStream* WINPR_RESTRICT out)
{
	const size_t step = 8 + ((context->common.format.nChannels > 1) ? 4 : 0);

	if (!Stream_EnsureRemainingCapacity(out, size))
		return FALSE;

	const size_t start = Stream_GetPosition(out);

	if (context->adpcm.ms.delta[0] < 16)
		context->adpcm.ms.delta[0] = 16;

	if (context->adpcm.ms.delta[1] < 16)
		context->adpcm.ms.delta[1] = 16;

	while (size >= step)
	{
		if ((Stream_GetPosition(out) - start) % context->common.format.nBlockAlign == 0)
		{
			if (context->common.format.nChannels > 1)
			{
				Stream_Write_UINT8(out, context->adpcm.ms.predictor[0]);
				Stream_Write_UINT8(out, context->adpcm.ms.predictor[1]);
				Stream_Write_UINT8(out, (context->adpcm.ms.delta[0] & 0xFF));
				Stream_Write_UINT8(out, ((context->adpcm.ms.delta[0] >> 8) & 0xFF));
				Stream_Write_UINT8(out, (context->adpcm.ms.delta[1] & 0xFF));
				Stream_Write_UINT8(out, ((context->adpcm.ms.delta[1] >> 8) & 0xFF));

				context->adpcm.ms.sample1[0] = read_int16(src + 4);
				context->adpcm.ms.sample1[1] = read_int16(src + 6);
				context->adpcm.ms.sample2[0] = read_int16(src + 0);
				context->adpcm.ms.sample2[1] = read_int16(src + 2);

				Stream_Write_INT16(out, (INT16)context->adpcm.ms.sample1[0]);
				Stream_Write_INT16(out, (INT16)context->adpcm.ms.sample1[1]);
				Stream_Write_INT16(out, (INT16)context->adpcm.ms.sample2[0]);
				Stream_Write_INT16(out, (INT16)context->adpcm.ms.sample2[1]);

				src += 8;
				size -= 8;
			}
			else
			{
				Stream_Write_UINT8(out, context->adpcm.ms.predictor[0]);
				Stream_Write_UINT8(out, (BYTE)(context->adpcm.ms.delta[0] & 0xFF));
				Stream_Write_UINT8(out, (BYTE)((context->adpcm.ms.delta[0] >> 8) & 0xFF));

				context->adpcm.ms.sample1[0] = read_int16(src + 2);
				context->adpcm.ms.sample2[0] = read_int16(src + 0);

				Stream_Write_INT16(out, (INT16)context->adpcm.ms.sample1[0]);
				Stream_Write_INT16(out, (INT16)context->adpcm.ms.sample2[0]);
				src += 4;
				size -= 4;
			}
		}

		{
			const INT16 sample = read_int16(src);
			src += 2;
			Stream_Write_UINT8(
			    out, (freerdp_dsp_encode_ms_adpcm_sample(&context->adpcm, sample, 0) << 4) & 0xFF);
		}
		{
			const INT16 sample = read_int16(src);
			src += 2;

			BYTE val = 0;
			Stream_Read_UINT8(out, val);
			val += freerdp_dsp_encode_ms_adpcm_sample(&context->adpcm, sample,
			                                          context->common.format.nChannels > 1 ? 1 : 0);
			Stream_Write_UINT8(out, val);
		}
		size -= 4;
	}

	return TRUE;
}

#endif

FREERDP_DSP_CONTEXT* freerdp_dsp_context_new(BOOL encoder)
{
#if defined(WITH_DSP_FFMPEG)
	return freerdp_dsp_ffmpeg_context_new(encoder);
#else
	FREERDP_DSP_CONTEXT* context = calloc(1, sizeof(FREERDP_DSP_CONTEXT));

	if (!context)
		return NULL;

	if (!freerdp_dsp_common_context_init(&context->common, encoder))
		goto fail;

#if defined(WITH_GSM)
	context->gsm = gsm_create();

	if (!context->gsm)
		goto fail;

	{
		int rc;
		int val = 1;
		rc = gsm_option(context->gsm, GSM_OPT_WAV49, &val);

		if (rc < 0)
			goto fail;
	}
#endif
#if defined(WITH_LAME)

	if (encoder)
	{
		context->lame = lame_init();

		if (!context->lame)
			goto fail;
	}
	else
	{
		context->hip = hip_decode_init();

		if (!context->hip)
			goto fail;
	}

#endif
#if defined(WITH_FAAD2)

	if (!encoder)
	{
		context->faad = NeAACDecOpen();

		if (!context->faad)
			goto fail;
	}

#endif
	return context;
fail:
	freerdp_dsp_context_free(context);
	return NULL;
#endif
}

void freerdp_dsp_context_free(FREERDP_DSP_CONTEXT* context)
{
	if (!context)
		return;

#if defined(WITH_FDK_AAC)
	FREERDP_DSP_COMMON_CONTEXT* ctx = (FREERDP_DSP_COMMON_CONTEXT*)context;
	WINPR_ASSERT(ctx);
	fdk_aac_dsp_uninit(ctx);
#endif

#if defined(WITH_DSP_FFMPEG)
	freerdp_dsp_ffmpeg_context_free(context);
#else

	freerdp_dsp_common_context_uninit(&context->common);

#if defined(WITH_GSM)
		gsm_destroy(context->gsm);
#endif
#if defined(WITH_LAME)

		if (context->common.encoder)
			lame_close(context->lame);
		else
			hip_decode_exit(context->hip);

#endif
#if defined(WITH_OPUS)

		if (context->opus_decoder)
			opus_decoder_destroy(context->opus_decoder);
		if (context->opus_encoder)
			opus_encoder_destroy(context->opus_encoder);

#endif
#if defined(WITH_FAAD2)

		if (!context->common.encoder)
			NeAACDecClose(context->faad);

#endif
#if defined(WITH_FAAC)

		if (context->faac)
			faacEncClose(context->faac);

#endif
#if defined(WITH_SOXR)
		soxr_delete(context->sox);
#endif
	    free(context);

#endif
}

BOOL freerdp_dsp_encode(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                        const AUDIO_FORMAT* WINPR_RESTRICT srcFormat,
                        const BYTE* WINPR_RESTRICT pdata, size_t length,
                        wStream* WINPR_RESTRICT out)
{
#if defined(WITH_FDK_AAC)
	FREERDP_DSP_COMMON_CONTEXT* ctx = (FREERDP_DSP_COMMON_CONTEXT*)context;
	WINPR_ASSERT(ctx);
	switch (ctx->format.wFormatTag)
	{
		case WAVE_FORMAT_AAC_MS:
			return fdk_aac_dsp_encode(ctx, srcFormat, pdata, length, out);
		default:
			break;
	}
#endif

#if defined(WITH_DSP_FFMPEG)
	return freerdp_dsp_ffmpeg_encode(context, srcFormat, pdata, length, out);
#else
	if (!context || !context->common.encoder || !srcFormat || !pdata || !out)
		return FALSE;

	AUDIO_FORMAT format = *srcFormat;
	const BYTE* resampleData = NULL;
	size_t resampleLength = 0;

	if (!freerdp_dsp_channel_mix(context, pdata, length, srcFormat, &resampleData, &resampleLength))
		return FALSE;

	format.nChannels = context->common.format.nChannels;

	const BYTE* data = NULL;
	if (!freerdp_dsp_resample(context, resampleData, resampleLength, &format, &data, &length))
		return FALSE;

	switch (context->common.format.wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			if (!Stream_EnsureRemainingCapacity(out, length))
				return FALSE;

			Stream_Write(out, data, length);
			return TRUE;

		case WAVE_FORMAT_ADPCM:
			return freerdp_dsp_encode_ms_adpcm(context, data, length, out);

		case WAVE_FORMAT_DVI_ADPCM:
			return freerdp_dsp_encode_ima_adpcm(context, data, length, out);
#if defined(WITH_GSM)

		case WAVE_FORMAT_GSM610:
			return freerdp_dsp_encode_gsm610(context, data, length, out);
#endif
#if defined(WITH_LAME)

		case WAVE_FORMAT_MPEGLAYER3:
			return freerdp_dsp_encode_mp3(context, data, length, out);
#endif
#if defined(WITH_FAAC)

		case WAVE_FORMAT_AAC_MS:
			return freerdp_dsp_encode_faac(context, data, length, out);
#endif
#if defined(WITH_OPUS)

		case WAVE_FORMAT_OPUS:
			return freerdp_dsp_encode_opus(context, data, length, out);
#endif
		default:
			return FALSE;
	}

	return FALSE;
#endif
}

BOOL freerdp_dsp_decode(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                        const AUDIO_FORMAT* WINPR_RESTRICT srcFormat,
                        const BYTE* WINPR_RESTRICT data, size_t length, wStream* WINPR_RESTRICT out)
{
#if defined(WITH_FDK_AAC)
	FREERDP_DSP_COMMON_CONTEXT* ctx = (FREERDP_DSP_COMMON_CONTEXT*)context;
	WINPR_ASSERT(ctx);
	switch (ctx->format.wFormatTag)
	{
		case WAVE_FORMAT_AAC_MS:
			return fdk_aac_dsp_decode(ctx, srcFormat, data, length, out);
		default:
			break;
	}
#endif

#if defined(WITH_DSP_FFMPEG)
	return freerdp_dsp_ffmpeg_decode(context, srcFormat, data, length, out);
#else

	if (!context || context->common.encoder || !srcFormat || !data || !out)
		return FALSE;

	switch (context->common.format.wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			if (!Stream_EnsureRemainingCapacity(out, length))
				return FALSE;

			Stream_Write(out, data, length);
			return TRUE;

		case WAVE_FORMAT_ADPCM:
			return freerdp_dsp_decode_ms_adpcm(context, data, length, out);

		case WAVE_FORMAT_DVI_ADPCM:
			return freerdp_dsp_decode_ima_adpcm(context, data, length, out);
#if defined(WITH_GSM)

		case WAVE_FORMAT_GSM610:
			return freerdp_dsp_decode_gsm610(context, data, length, out);
#endif
#if defined(WITH_LAME)

		case WAVE_FORMAT_MPEGLAYER3:
			return freerdp_dsp_decode_mp3(context, data, length, out);
#endif
#if defined(WITH_FAAD2)

		case WAVE_FORMAT_AAC_MS:
			return freerdp_dsp_decode_faad(context, data, length, out);
#endif

#if defined(WITH_OPUS)
		case WAVE_FORMAT_OPUS:
			return freerdp_dsp_decode_opus(context, data, length, out);
#endif
		default:
			return FALSE;
	}

	return FALSE;
#endif
}

BOOL freerdp_dsp_supports_format(const AUDIO_FORMAT* WINPR_RESTRICT format, BOOL encode)
{
#if defined(WITH_FDK_AAC)
	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_AAC_MS:
			return TRUE;
		default:
			break;
	}

#endif

#if defined(WITH_DSP_FFMPEG)
	return freerdp_dsp_ffmpeg_supports_format(format, encode);
#else

#if !defined(WITH_DSP_EXPERIMENTAL)
	WINPR_UNUSED(encode);
#endif
	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			return TRUE;
#if defined(WITH_DSP_EXPERIMENTAL)

		case WAVE_FORMAT_ADPCM:
			return FALSE;
		case WAVE_FORMAT_DVI_ADPCM:
			return TRUE;
#endif
#if defined(WITH_GSM)

		case WAVE_FORMAT_GSM610:
#if defined(WITH_DSP_EXPERIMENTAL)
			return TRUE;
#else
			return !encode;
#endif
#endif
#if defined(WITH_LAME)

		case WAVE_FORMAT_MPEGLAYER3:
#if defined(WITH_DSP_EXPERIMENTAL)
			return TRUE;
#else
			return !encode;
#endif
#endif

		case WAVE_FORMAT_AAC_MS:
#if defined(WITH_FAAD2)
			if (!encode)
				return TRUE;

#endif
#if defined(WITH_FAAC)

			if (encode)
				return TRUE;

#endif
#if defined(WITH_OPUS)
			/* fallthrough */
			WINPR_FALLTHROUGH
		case WAVE_FORMAT_OPUS:
			return opus_is_valid_samplerate(format);
#endif
			/* fallthrough */
			WINPR_FALLTHROUGH
		default:
			return FALSE;
	}

	return FALSE;
#endif
}

BOOL freerdp_dsp_context_reset(FREERDP_DSP_CONTEXT* WINPR_RESTRICT context,
                               const AUDIO_FORMAT* WINPR_RESTRICT targetFormat,
                               UINT32 FramesPerPacket)
{
#if defined(WITH_FDK_AAC)
	WINPR_ASSERT(targetFormat);
	if (targetFormat->wFormatTag == WAVE_FORMAT_AAC_MS)
	{
		FREERDP_DSP_COMMON_CONTEXT* ctx = (FREERDP_DSP_COMMON_CONTEXT*)context;
		fdk_aac_dsp_uninit(ctx);
		ctx->format = *targetFormat;
		return fdk_aac_dsp_init(ctx, FramesPerPacket);
	}
#endif

#if defined(WITH_DSP_FFMPEG)
	return freerdp_dsp_ffmpeg_context_reset(context, targetFormat);
#else

	if (!context || !targetFormat)
		return FALSE;

	context->common.format = *targetFormat;

	if (context->common.format.wFormatTag == WAVE_FORMAT_DVI_ADPCM)
	{
		size_t min_frame_data = 1ull * context->common.format.wBitsPerSample *
		                        context->common.format.nChannels * FramesPerPacket;
		size_t data_per_block =
		    (1ULL * context->common.format.nBlockAlign - 4ULL * context->common.format.nChannels) *
		    8ULL;
		size_t nb_block_per_packet = min_frame_data / data_per_block;

		if (min_frame_data % data_per_block)
			nb_block_per_packet++;

		context->adpcm.ima.packet_size = nb_block_per_packet * context->common.format.nBlockAlign;
		Stream_EnsureCapacity(context->common.buffer, context->adpcm.ima.packet_size);
		Stream_SetPosition(context->common.buffer, 0);
	}

#if defined(WITH_OPUS)

	if (opus_is_valid_samplerate(&context->common.format))
	{
		if (!context->common.encoder)
		{
			int opus_error = OPUS_OK;

			context->opus_decoder =
			    opus_decoder_create(context->common.format.nSamplesPerSec,
			                        context->common.format.nChannels, &opus_error);
			if (opus_error != OPUS_OK)
				return FALSE;
		}
		else
		{
			int opus_error = OPUS_OK;

			context->opus_encoder = opus_encoder_create(context->common.format.nSamplesPerSec,
			                                            context->common.format.nChannels,
			                                            OPUS_APPLICATION_VOIP, &opus_error);
			if (opus_error != OPUS_OK)
				return FALSE;

			opus_error =
			    opus_encoder_ctl(context->opus_encoder,
			                     OPUS_SET_BITRATE(context->common.format.nAvgBytesPerSec * 8));
			if (opus_error != OPUS_OK)
				return FALSE;
		}
	}

#endif
#if defined(WITH_FAAD2)
	context->faadSetup = FALSE;
#endif
#if defined(WITH_FAAC)

	if (context->common.encoder)
	{
		faacEncConfigurationPtr cfg;

		if (context->faac)
			faacEncClose(context->faac);

		context->faac = faacEncOpen(targetFormat->nSamplesPerSec, targetFormat->nChannels,
		                            &context->faacInputSamples, &context->faacMaxOutputBytes);

		if (!context->faac)
			return FALSE;

		cfg = faacEncGetCurrentConfiguration(context->faac);
		cfg->inputFormat = FAAC_INPUT_16BIT;
		cfg->outputFormat = 0;
		cfg->mpegVersion = MPEG4;
		cfg->useTns = 1;
		cfg->bandWidth = targetFormat->nAvgBytesPerSec;
		faacEncSetConfiguration(context->faac, cfg);
	}

#endif
#if defined(WITH_SOXR)
	{
		soxr_io_spec_t iospec = soxr_io_spec(SOXR_INT16, SOXR_INT16);
		soxr_error_t error;
		soxr_delete(context->sox);
		context->sox =
		    soxr_create(context->common.format.nSamplesPerSec, targetFormat->nSamplesPerSec,
		                targetFormat->nChannels, &error, &iospec, NULL, NULL);

		if (!context->sox || (error != 0))
			return FALSE;
	}
#endif
	return TRUE;
#endif
}

BOOL freerdp_dsp_common_context_init(FREERDP_DSP_COMMON_CONTEXT* context, BOOL encode)
{
	WINPR_ASSERT(context);
	context->encoder = encode;
	context->buffer = Stream_New(NULL, 1024);
	if (!context->buffer)
		goto fail;

	context->channelmix = Stream_New(NULL, 1024);
	if (!context->channelmix)
		goto fail;

	context->resample = Stream_New(NULL, 1024);
	if (!context->resample)
		goto fail;

	return TRUE;

fail:
	freerdp_dsp_common_context_uninit(context);
	return FALSE;
}

void freerdp_dsp_common_context_uninit(FREERDP_DSP_COMMON_CONTEXT* context)
{
	WINPR_ASSERT(context);

	Stream_Free(context->buffer, TRUE);
	Stream_Free(context->channelmix, TRUE);
	Stream_Free(context->resample, TRUE);

	context->buffer = NULL;
	context->channelmix = NULL;
	context->resample = NULL;
}
