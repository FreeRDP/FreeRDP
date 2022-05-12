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

#if !defined(WITH_DSP_FFMPEG)
#if defined(WITH_GSM)
#include <gsm/gsm.h>
#endif

#if defined(WITH_LAME)
#include <lame/lame.h>
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

#define TAG FREERDP_TAG("dsp")

#if !defined(WITH_DSP_FFMPEG)

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
	BOOL encoder;

	ADPCM adpcm;
	AUDIO_FORMAT format;

	wStream* channelmix;
	wStream* resample;
	wStream* buffer;

#if defined(WITH_GSM)
	gsm gsm;
#endif
#if defined(WITH_LAME)
	lame_t lame;
	hip_t hip;
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

static INT16 read_int16(const BYTE* src)
{
	return (INT16)(src[0] | (src[1] << 8));
}

static BOOL freerdp_dsp_channel_mix(FREERDP_DSP_CONTEXT* context, const BYTE* src, size_t size,
                                    const AUDIO_FORMAT* srcFormat, const BYTE** data,
                                    size_t* length)
{
	UINT32 bpp;
	size_t samples;
	size_t x, y;

	if (!context || !data || !length)
		return FALSE;

	if (srcFormat->wFormatTag != WAVE_FORMAT_PCM)
		return FALSE;

	bpp = srcFormat->wBitsPerSample > 8 ? 2 : 1;
	samples = size / bpp / srcFormat->nChannels;

	if (context->format.nChannels == srcFormat->nChannels)
	{
		*data = src;
		*length = size;
		return TRUE;
	}

	Stream_SetPosition(context->channelmix, 0);

	/* Destination has more channels than source */
	if (context->format.nChannels > srcFormat->nChannels)
	{
		switch (srcFormat->nChannels)
		{
			case 1:
				if (!Stream_EnsureCapacity(context->channelmix, size * 2))
					return FALSE;

				for (x = 0; x < samples; x++)
				{
					for (y = 0; y < bpp; y++)
						Stream_Write_UINT8(context->channelmix, src[x * bpp + y]);

					for (y = 0; y < bpp; y++)
						Stream_Write_UINT8(context->channelmix, src[x * bpp + y]);
				}

				Stream_SealLength(context->channelmix);
				*data = Stream_Buffer(context->channelmix);
				*length = Stream_Length(context->channelmix);
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
			if (!Stream_EnsureCapacity(context->channelmix, size / 2))
				return FALSE;

			/* Simply drop second channel.
			 * TODO: Calculate average */
			for (x = 0; x < samples; x++)
			{
				for (y = 0; y < bpp; y++)
					Stream_Write_UINT8(context->channelmix, src[2 * x * bpp + y]);
			}

			Stream_SealLength(context->channelmix);
			*data = Stream_Buffer(context->channelmix);
			*length = Stream_Length(context->channelmix);
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

static BOOL freerdp_dsp_resample(FREERDP_DSP_CONTEXT* context, const BYTE* src, size_t size,
                                 const AUDIO_FORMAT* srcFormat, const BYTE** data, size_t* length)
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
		WLog_ERR(TAG, "%s requires %s for sample input, got %s", __FUNCTION__,
		         audio_format_get_tag_string(WAVE_FORMAT_PCM),
		         audio_format_get_tag_string(srcFormat->wFormatTag));
		return FALSE;
	}

	/* We want to ignore differences of source and destination format. */
	format = *srcFormat;
	format.wFormatTag = WAVE_FORMAT_UNKNOWN;
	format.wBitsPerSample = 0;

	if (audio_format_compatible(&format, &context->format))
	{
		*data = src;
		*length = size;
		return TRUE;
	}

#if defined(WITH_SOXR)
	srcBytesPerFrame = (srcFormat->wBitsPerSample > 8) ? 2 : 1;
	dstBytesPerFrame = (context->format.wBitsPerSample > 8) ? 2 : 1;
	srcChannels = srcFormat->nChannels;
	dstChannels = context->format.nChannels;
	sbytes = srcChannels * srcBytesPerFrame;
	sframes = size / sbytes;
	rbytes = dstBytesPerFrame * dstChannels;
	/* Integer rounding correct division */
	rframes = (sframes * context->format.nSamplesPerSec + (srcFormat->nSamplesPerSec + 1) / 2) /
	          srcFormat->nSamplesPerSec;
	rsize = rframes * rbytes;

	if (!Stream_EnsureCapacity(context->resample, rsize))
		return FALSE;

	error = soxr_process(context->sox, src, sframes, &idone, Stream_Buffer(context->resample),
	                     Stream_Capacity(context->resample) / rbytes, &odone);
	Stream_SetLength(context->resample, odone * rbytes);
	*data = Stream_Buffer(context->resample);
	*length = Stream_Length(context->resample);
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

static UINT16 dsp_decode_ima_adpcm_sample(ADPCM* adpcm, unsigned int channel, BYTE sample)
{
	INT32 ss;
	INT32 d;
	ss = ima_step_size_table[adpcm->ima.last_step[channel]];
	d = (ss >> 3);

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

static BOOL freerdp_dsp_decode_ima_adpcm(FREERDP_DSP_CONTEXT* context, const BYTE* src, size_t size,
                                         wStream* out)
{
	BYTE sample;
	UINT16 decoded;
	size_t out_size = size * 4;
	UINT32 channel;
	const UINT32 block_size = context->format.nBlockAlign;
	const UINT32 channels = context->format.nChannels;
	size_t i;

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
			for (i = 0; i < 8; i++)
			{
				BYTE* dst = Stream_Pointer(out);

				channel = (i < 4 ? 0 : 1);
				sample = ((*src) & 0x0f);
				decoded = dsp_decode_ima_adpcm_sample(&context->adpcm, channel, sample);
				dst[((i & 3) << 3) + (channel << 1)] = (decoded & 0xFF);
				dst[((i & 3) << 3) + (channel << 1) + 1] = (decoded >> 8);
				sample = ((*src) >> 4);
				decoded = dsp_decode_ima_adpcm_sample(&context->adpcm, channel, sample);
				dst[((i & 3) << 3) + (channel << 1) + 4] = (decoded & 0xFF);
				dst[((i & 3) << 3) + (channel << 1) + 5] = (decoded >> 8);
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

			sample = ((*src) & 0x0f);
			decoded = dsp_decode_ima_adpcm_sample(&context->adpcm, 0, sample);
			*dst++ = (decoded & 0xFF);
			*dst++ = (decoded >> 8);
			sample = ((*src) >> 4);
			decoded = dsp_decode_ima_adpcm_sample(&context->adpcm, 0, sample);
			*dst++ = (decoded & 0xFF);
			*dst++ = (decoded >> 8);
			src++;
			size--;
		}
	}

	return TRUE;
}

#if defined(WITH_GSM)
static BOOL freerdp_dsp_decode_gsm610(FREERDP_DSP_CONTEXT* context, const BYTE* src, size_t size,
                                      wStream* out)
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

static BOOL freerdp_dsp_encode_gsm610(FREERDP_DSP_CONTEXT* context, const BYTE* src, size_t size,
                                      wStream* out)
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
static BOOL freerdp_dsp_decode_mp3(FREERDP_DSP_CONTEXT* context, const BYTE* src, size_t size,
                                   wStream* out)
{
	int rc, x;
	short* pcm_l;
	short* pcm_r;
	size_t buffer_size;

	if (!context || !src || !out)
		return FALSE;

	buffer_size = 2 * context->format.nChannels * context->format.nSamplesPerSec;

	if (!Stream_EnsureCapacity(context->buffer, 2 * buffer_size))
		return FALSE;

	pcm_l = (short*)Stream_Buffer(context->buffer);
	pcm_r = (short*)Stream_Buffer(context->buffer) + buffer_size;
	rc = hip_decode(context->hip, (unsigned char*)/* API is not modifying content */ src, size,
	                pcm_l, pcm_r);

	if (rc <= 0)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(out, (size_t)rc * context->format.nChannels * 2))
		return FALSE;

	for (x = 0; x < rc; x++)
	{
		Stream_Write_UINT16(out, (UINT16)pcm_l[x]);
		Stream_Write_UINT16(out, (UINT16)pcm_r[x]);
	}

	return TRUE;
}

static BOOL freerdp_dsp_encode_mp3(FREERDP_DSP_CONTEXT* context, const BYTE* src, size_t size,
                                   wStream* out)
{
	size_t samples_per_channel;
	int rc;

	if (!context || !src || !out)
		return FALSE;

	samples_per_channel = size / context->format.nChannels / context->format.wBitsPerSample / 8;

	/* Ensure worst case buffer size for mp3 stream taken from LAME header */
	if (!Stream_EnsureRemainingCapacity(out, 5 / 4 * samples_per_channel + 7200))
		return FALSE;

	samples_per_channel = size / 2 /* size of a sample */ / context->format.nChannels;
	rc = lame_encode_buffer_interleaved(context->lame, (short*)src, samples_per_channel,
	                                    Stream_Pointer(out), Stream_GetRemainingCapacity(out));

	if (rc < 0)
		return FALSE;

	Stream_Seek(out, (size_t)rc);
	return TRUE;
}
#endif

#if defined(WITH_FAAC)
static BOOL freerdp_dsp_encode_faac(FREERDP_DSP_CONTEXT* context, const BYTE* src, size_t size,
                                    wStream* out)
{
	const int16_t* inSamples = (const int16_t*)src;
	unsigned int bpp;
	size_t nrSamples, x;
	int rc;

	if (!context || !src || !out)
		return FALSE;

	bpp = context->format.wBitsPerSample / 8;
	nrSamples = size / bpp;

	if (!Stream_EnsureRemainingCapacity(context->buffer, nrSamples * sizeof(int16_t)))
		return FALSE;

	for (x = 0; x < nrSamples; x++)
	{
		Stream_Write_INT16(context->buffer, inSamples[x]);
		if (Stream_GetPosition(context->buffer) / bpp >= context->faacInputSamples)
		{
			if (!Stream_EnsureRemainingCapacity(out, context->faacMaxOutputBytes))
				return FALSE;
			rc = faacEncEncode(context->faac, (int32_t*)Stream_Buffer(context->buffer),
			                   context->faacInputSamples, Stream_Pointer(out),
			                   Stream_GetRemainingCapacity(out));
			if (rc < 0)
				return FALSE;
			if (rc > 0)
				Stream_Seek(out, (size_t)rc);
			Stream_SetPosition(context->buffer, 0);
		}
	}

	return TRUE;
}
#endif

#if defined(WITH_FAAD2)
static BOOL freerdp_dsp_decode_faad(FREERDP_DSP_CONTEXT* context, const BYTE* src, size_t size,
                                    wStream* out)
{
	NeAACDecFrameInfo info;
	void* output;
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

		if (channels != context->format.nChannels)
			return FALSE;

		if (samplerate != context->format.nSamplesPerSec)
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
		outSize = context->format.nSamplesPerSec * context->format.nChannels *
		          context->format.wBitsPerSample / 8;

		if (!Stream_EnsureRemainingCapacity(out, outSize))
			return FALSE;

		sample_buffer = Stream_Pointer(out);

		cnv.cpv = &src[offset];
		output = NeAACDecDecode2(context->faad, &info, cnv.pv, size - offset, &sample_buffer,
		                         Stream_GetRemainingCapacity(out));

		if (info.error != 0)
			return FALSE;

		offset += info.bytesconsumed;

		if (info.samples == 0)
			continue;

		Stream_Seek(out, info.samples * context->format.wBitsPerSample / 8);
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

static BYTE dsp_encode_ima_adpcm_sample(ADPCM* adpcm, int channel, INT16 sample)
{
	INT32 e;
	INT32 d;
	INT32 ss;
	BYTE enc;
	INT32 diff;
	ss = ima_step_size_table[adpcm->ima.last_step[channel]];
	d = e = sample - adpcm->ima.last_sample[channel];
	diff = ss >> 3;
	enc = 0;

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

static BOOL freerdp_dsp_encode_ima_adpcm(FREERDP_DSP_CONTEXT* context, const BYTE* src, size_t size,
                                         wStream* out)
{
	int i;
	size_t start;
	INT16 sample;
	BYTE encoded;
	size_t align;

	if (!Stream_EnsureRemainingCapacity(out, size))
		return FALSE;

	start = Stream_GetPosition(context->buffer);

	align = (context->format.nChannels > 1) ? 32 : 4;

	while (size >= align)
	{
		if ((Stream_GetPosition(context->buffer) - start) % context->format.nBlockAlign == 0)
		{
			Stream_Write_UINT8(context->buffer, context->adpcm.ima.last_sample[0] & 0xFF);
			Stream_Write_UINT8(context->buffer, (context->adpcm.ima.last_sample[0] >> 8) & 0xFF);
			Stream_Write_UINT8(context->buffer, (BYTE)context->adpcm.ima.last_step[0]);
			Stream_Write_UINT8(context->buffer, 0);

			if (context->format.nChannels > 1)
			{
				Stream_Write_UINT8(context->buffer, context->adpcm.ima.last_sample[1] & 0xFF);
				Stream_Write_UINT8(context->buffer,
				                   (context->adpcm.ima.last_sample[1] >> 8) & 0xFF);
				Stream_Write_UINT8(context->buffer, (BYTE)context->adpcm.ima.last_step[1]);
				Stream_Write_UINT8(context->buffer, 0);
			}
		}

		if (context->format.nChannels > 1)
		{
			BYTE* dst = Stream_Pointer(context->buffer);
			ZeroMemory(dst, 8);

			for (i = 0; i < 16; i++)
			{
				sample = (INT16)(((UINT16)(*src)) | (((UINT16)(*(src + 1))) << 8));
				src += 2;
				encoded = dsp_encode_ima_adpcm_sample(&context->adpcm, i % 2, sample);
				dst[ima_stereo_encode_map[i].byte_num] |= encoded
				                                          << ima_stereo_encode_map[i].byte_shift;
			}

			if (!Stream_SafeSeek(context->buffer, 8))
				return FALSE;
			size -= 32;
		}
		else
		{
			sample = (INT16)(((UINT16)(*src)) | (((UINT16)(*(src + 1))) << 8));
			src += 2;
			encoded = dsp_encode_ima_adpcm_sample(&context->adpcm, 0, sample);
			sample = (INT16)(((UINT16)(*src)) | (((UINT16)(*(src + 1))) << 8));
			src += 2;
			encoded |= dsp_encode_ima_adpcm_sample(&context->adpcm, 0, sample) << 4;
			Stream_Write_UINT8(context->buffer, encoded);
			size -= 4;
		}

		if (Stream_GetPosition(context->buffer) - start == context->adpcm.ima.packet_size)
		{
			BYTE* bsrc = Stream_Buffer(context->buffer);
			Stream_Write(out, bsrc, context->adpcm.ima.packet_size);
			Stream_SetPosition(context->buffer, 0);
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

static INLINE INT16 freerdp_dsp_decode_ms_adpcm_sample(ADPCM* adpcm, BYTE sample, int channel)
{
	INT8 nibble;
	INT32 presample;
	nibble = (sample & 0x08 ? (INT8)sample - 16 : (INT8)sample);
	presample = ((adpcm->ms.sample1[channel] * ms_adpcm_coeffs1[adpcm->ms.predictor[channel]]) +
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

static BOOL freerdp_dsp_decode_ms_adpcm(FREERDP_DSP_CONTEXT* context, const BYTE* src, size_t size,
                                        wStream* out)
{
	BYTE sample;
	const size_t out_size = size * 4;
	const UINT32 channels = context->format.nChannels;
	const UINT32 block_size = context->format.nBlockAlign;

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
			sample = *src++;
			size--;
			Stream_Write_INT16(out,
			                   freerdp_dsp_decode_ms_adpcm_sample(&context->adpcm, sample >> 4, 0));
			Stream_Write_INT16(
			    out, freerdp_dsp_decode_ms_adpcm_sample(&context->adpcm, sample & 0x0F, 1));
			sample = *src++;
			size--;
			Stream_Write_INT16(out,
			                   freerdp_dsp_decode_ms_adpcm_sample(&context->adpcm, sample >> 4, 0));
			Stream_Write_INT16(
			    out, freerdp_dsp_decode_ms_adpcm_sample(&context->adpcm, sample & 0x0F, 1));
		}
		else
		{
			sample = *src++;
			size--;
			Stream_Write_INT16(out,
			                   freerdp_dsp_decode_ms_adpcm_sample(&context->adpcm, sample >> 4, 0));
			Stream_Write_INT16(
			    out, freerdp_dsp_decode_ms_adpcm_sample(&context->adpcm, sample & 0x0F, 0));
		}
	}

	return TRUE;
}

static BYTE freerdp_dsp_encode_ms_adpcm_sample(ADPCM* adpcm, INT32 sample, int channel)
{
	INT32 presample;
	INT32 errordelta;
	presample = ((adpcm->ms.sample1[channel] * ms_adpcm_coeffs1[adpcm->ms.predictor[channel]]) +
	             (adpcm->ms.sample2[channel] * ms_adpcm_coeffs2[adpcm->ms.predictor[channel]])) /
	            256;
	errordelta = (sample - presample) / adpcm->ms.delta[channel];

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

static BOOL freerdp_dsp_encode_ms_adpcm(FREERDP_DSP_CONTEXT* context, const BYTE* src, size_t size,
                                        wStream* out)
{
	size_t start;
	INT32 sample;
	const size_t step = 8 + ((context->format.nChannels > 1) ? 4 : 0);

	if (!Stream_EnsureRemainingCapacity(out, size))
		return FALSE;

	start = Stream_GetPosition(out);

	if (context->adpcm.ms.delta[0] < 16)
		context->adpcm.ms.delta[0] = 16;

	if (context->adpcm.ms.delta[1] < 16)
		context->adpcm.ms.delta[1] = 16;

	while (size >= step)
	{
		BYTE val;
		if ((Stream_GetPosition(out) - start) % context->format.nBlockAlign == 0)
		{
			if (context->format.nChannels > 1)
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

		sample = read_int16(src);
		src += 2;
		Stream_Write_UINT8(
		    out, (freerdp_dsp_encode_ms_adpcm_sample(&context->adpcm, sample, 0) << 4) & 0xFF);
		sample = read_int16(src);
		src += 2;

		Stream_Read_UINT8(out, val);
		val += freerdp_dsp_encode_ms_adpcm_sample(&context->adpcm, sample,
		                                          context->format.nChannels > 1 ? 1 : 0);
		Stream_Write_UINT8(out, val);
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

	context->channelmix = Stream_New(NULL, 4096);

	if (!context->channelmix)
		goto fail;

	context->resample = Stream_New(NULL, 4096);

	if (!context->resample)
		goto fail;

	context->buffer = Stream_New(NULL, 4096);

	if (!context->buffer)
		goto fail;

	context->encoder = encoder;
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
#if defined(WITH_DSP_FFMPEG)
	freerdp_dsp_ffmpeg_context_free(context);
#else

	if (context)
	{
		Stream_Free(context->channelmix, TRUE);
		Stream_Free(context->resample, TRUE);
		Stream_Free(context->buffer, TRUE);
#if defined(WITH_GSM)
		gsm_destroy(context->gsm);
#endif
#if defined(WITH_LAME)

		if (context->encoder)
			lame_close(context->lame);
		else
			hip_decode_exit(context->hip);

#endif
#if defined(WITH_FAAD2)

		if (!context->encoder)
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
	}

#endif
}

BOOL freerdp_dsp_encode(FREERDP_DSP_CONTEXT* context, const AUDIO_FORMAT* srcFormat,
                        const BYTE* data, size_t length, wStream* out)
{
#if defined(WITH_DSP_FFMPEG)
	return freerdp_dsp_ffmpeg_encode(context, srcFormat, data, length, out);
#else
	const BYTE* resampleData;
	size_t resampleLength;
	AUDIO_FORMAT format;

	if (!context || !context->encoder || !srcFormat || !data || !out)
		return FALSE;

	format = *srcFormat;

	if (!freerdp_dsp_channel_mix(context, data, length, srcFormat, &resampleData, &resampleLength))
		return FALSE;

	format.nChannels = context->format.nChannels;

	if (!freerdp_dsp_resample(context, resampleData, resampleLength, &format, &data, &length))
		return FALSE;

	switch (context->format.wFormatTag)
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

		default:
			return FALSE;
	}

	return FALSE;
#endif
}

BOOL freerdp_dsp_decode(FREERDP_DSP_CONTEXT* context, const AUDIO_FORMAT* srcFormat,
                        const BYTE* data, size_t length, wStream* out)
{
#if defined(WITH_DSP_FFMPEG)
	return freerdp_dsp_ffmpeg_decode(context, srcFormat, data, length, out);
#else

	if (!context || context->encoder || !srcFormat || !data || !out)
		return FALSE;

	switch (context->format.wFormatTag)
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

		default:
			return FALSE;
	}

	return FALSE;
#endif
}

BOOL freerdp_dsp_supports_format(const AUDIO_FORMAT* format, BOOL encode)
{
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

		default:
			return FALSE;
	}

	return FALSE;
#endif
}

BOOL freerdp_dsp_context_reset(FREERDP_DSP_CONTEXT* context, const AUDIO_FORMAT* targetFormat,
                               UINT32 FramesPerPacket)
{
#if defined(WITH_DSP_FFMPEG)
	return freerdp_dsp_ffmpeg_context_reset(context, targetFormat);
#else

	if (!context || !targetFormat)
		return FALSE;

	context->format = *targetFormat;

	if (context->format.wFormatTag == WAVE_FORMAT_DVI_ADPCM)
	{
		size_t min_frame_data =
		    context->format.wBitsPerSample * context->format.nChannels * FramesPerPacket * 1ULL;
		size_t data_per_block = (context->format.nBlockAlign - 4 * context->format.nChannels) * 8;
		size_t nb_block_per_packet = min_frame_data / data_per_block;

		if (min_frame_data % data_per_block)
			nb_block_per_packet++;

		context->adpcm.ima.packet_size = nb_block_per_packet * context->format.nBlockAlign;
		Stream_EnsureCapacity(context->buffer, context->adpcm.ima.packet_size);
		Stream_SetPosition(context->buffer, 0);
	}

#if defined(WITH_FAAD2)
	context->faadSetup = FALSE;
#endif
#if defined(WITH_FAAC)

	if (context->encoder)
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
		context->sox = soxr_create(context->format.nSamplesPerSec, targetFormat->nSamplesPerSec,
		                           targetFormat->nChannels, &error, &iospec, NULL, NULL);

		if (!context->sox || (error != 0))
			return FALSE;
	}
#endif
	return TRUE;
#endif
}
