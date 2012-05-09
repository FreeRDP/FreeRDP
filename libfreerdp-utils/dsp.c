/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/dsp.h>

static void freerdp_dsp_resample(FREERDP_DSP_CONTEXT* context,
	const uint8* src, int bytes_per_sample,
	uint32 schan, uint32 srate, int sframes,
	uint32 rchan, uint32 rrate)
{
	uint8* dst;
	uint8* p;
	int rframes;
	int rsize;
	int i, j;
	int n1, n2;
	int sbytes, rbytes;

	sbytes = bytes_per_sample * schan;
	rbytes = bytes_per_sample * rchan;
	rframes = sframes * rrate / srate;
	rsize = rbytes * rframes;
	if (rsize > context->resampled_maxlength)
	{
		context->resampled_maxlength = rsize + 1024;
		context->resampled_buffer = xrealloc(context->resampled_buffer, context->resampled_maxlength);
	}
	dst = context->resampled_buffer;

	p = dst;
	for (i = 0; i < rframes; i++)
	{
		n1 = i * srate / rrate;
		if (n1 >= sframes)
			n1 = sframes - 1;
		n2 = (n1 * rrate == i * srate || n1 == sframes - 1 ? n1 : n1 + 1);
		for (j = 0; j < rbytes; j++)
		{
			/* Nearest Interpolation, probably the easiest, but works */
			*p++ = (i * srate - n1 * rrate > n2 * rrate - i * srate ?
				src[n2 * sbytes + (j % sbytes)] :
				src[n1 * sbytes + (j % sbytes)]);
		}
	}

	context->resampled_frames = rframes;
	context->resampled_size = rsize;
}

/**
 * Microsoft IMA ADPCM specification:
 *
 * http://wiki.multimedia.cx/index.php?title=Microsoft_IMA_ADPCM
 * http://wiki.multimedia.cx/index.php?title=IMA_ADPCM
 */

static const sint16 ima_step_index_table[] =
{
	-1, -1, -1, -1, 2, 4, 6, 8,
	-1, -1, -1, -1, 2, 4, 6, 8
};

static const sint16 ima_step_size_table[] =
{
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 
	19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 
	130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
	337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
	876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 
	2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
	5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 
};

static uint16 dsp_decode_ima_adpcm_sample(ADPCM* adpcm,
	int channel, uint8 sample)
{
	sint32 ss;
	sint32 d;

	ss = ima_step_size_table[adpcm->last_step[channel]];
	d = (ss >> 3);
	if (sample & 1)
		d += (ss >> 2);
	if (sample & 2)
		d += (ss >> 1);
	if (sample & 4)
		d += ss;
	if (sample & 8)
		d = -d;
	d += adpcm->last_sample[channel];

	if (d < -32768)
		d = -32768;
	else if (d > 32767)
		d = 32767;

	adpcm->last_sample[channel] = (sint16) d;

	adpcm->last_step[channel] += ima_step_index_table[sample];
	if (adpcm->last_step[channel] < 0)
		adpcm->last_step[channel] = 0;
	else if (adpcm->last_step[channel] > 88)
		adpcm->last_step[channel] = 88;

	return (uint16) d;
}

static void freerdp_dsp_decode_ima_adpcm(FREERDP_DSP_CONTEXT* context,
	const uint8* src, int size, int channels, int block_size)
{
	uint8* dst;
	uint8 sample;
	uint16 decoded;
	uint32 out_size;
	int channel;
	int i;

	out_size = size * 4;
	if (out_size > context->adpcm_maxlength)
	{
		context->adpcm_maxlength = out_size + 1024;
		context->adpcm_buffer = xrealloc(context->adpcm_buffer, context->adpcm_maxlength);
	}
	dst = context->adpcm_buffer;
	while (size > 0)
	{
		if (size % block_size == 0)
		{
			context->adpcm.last_sample[0] = (sint16) (((uint16)(*src)) | (((uint16)(*(src + 1))) << 8));
			context->adpcm.last_step[0] = (sint16) (*(src + 2));
			src += 4;
			size -= 4;
			out_size -= 16;
			if (channels > 1)
			{
				context->adpcm.last_sample[1] = (sint16) (((uint16)(*src)) | (((uint16)(*(src + 1))) << 8));
				context->adpcm.last_step[1] = (sint16) (*(src + 2));
				src += 4;
				size -= 4;
				out_size -= 16;
			}
		}

		if (channels > 1)
		{
			for (i = 0; i < 8; i++)
			{
				channel = (i < 4 ? 0 : 1);
				sample = ((*src) & 0x0f);
				decoded = dsp_decode_ima_adpcm_sample(&context->adpcm, channel, sample);
				dst[((i & 3) << 3) + (channel << 1)] = (decoded & 0xff);
				dst[((i & 3) << 3) + (channel << 1) + 1] = (decoded >> 8);
				sample = ((*src) >> 4);
				decoded = dsp_decode_ima_adpcm_sample(&context->adpcm, channel, sample);
				dst[((i & 3) << 3) + (channel << 1) + 4] = (decoded & 0xff);
				dst[((i & 3) << 3) + (channel << 1) + 5] = (decoded >> 8);
				src++;
			}
			dst += 32;
			size -= 8;
		}
		else
		{
			sample = ((*src) & 0x0f);
			decoded = dsp_decode_ima_adpcm_sample(&context->adpcm, 0, sample);
			*dst++ = (decoded & 0xff);
			*dst++ = (decoded >> 8);
			sample = ((*src) >> 4);
			decoded = dsp_decode_ima_adpcm_sample(&context->adpcm, 0, sample);
			*dst++ = (decoded & 0xff);
			*dst++ = (decoded >> 8);
			src++;
			size--;
		}
	}

	context->adpcm_size = out_size;
}

/**
 * 0     1     2     3
 * 2 0   6 4   10 8  14 12   <left>
 *
 * 4     5     6     7
 * 3 1   7 5   11 9  15 13   <right>
 */
static const struct
{
	uint8 byte_num;
	uint8 byte_shift;
} ima_stereo_encode_map[] =
{
	{ 0, 0 },
	{ 4, 0 },
	{ 0, 4 },
	{ 4, 4 },
	{ 1, 0 },
	{ 5, 0 },
	{ 1, 4 },
	{ 5, 4 },
	{ 2, 0 },
	{ 6, 0 },
	{ 2, 4 },
	{ 6, 4 },
	{ 3, 0 },
	{ 7, 0 },
	{ 3, 4 },
	{ 7, 4 }
};

static uint8 dsp_encode_ima_adpcm_sample(ADPCM* adpcm,
	int channel, sint16 sample)
{
	sint32 e;
	sint32 d;
	sint32 ss;
	uint8 enc;
	sint32 diff;

	ss = ima_step_size_table[adpcm->last_step[channel]];
	d = e = sample - adpcm->last_sample[channel];
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

	diff += adpcm->last_sample[channel];
	if (diff < -32768)
		diff = -32768;
	else if (diff > 32767)
		diff = 32767;
	adpcm->last_sample[channel] = (sint16) diff;

	adpcm->last_step[channel] += ima_step_index_table[enc];
	if (adpcm->last_step[channel] < 0)
		adpcm->last_step[channel] = 0;
	else if (adpcm->last_step[channel] > 88)
		adpcm->last_step[channel] = 88;

	return enc;
}

static void freerdp_dsp_encode_ima_adpcm(FREERDP_DSP_CONTEXT* context,
	const uint8* src, int size, int channels, int block_size)
{
	uint8* dst;
	sint16 sample;
	uint8 encoded;
	uint32 out_size;
	int i;

	out_size = size / 2;
	if (out_size > context->adpcm_maxlength)
	{
		context->adpcm_maxlength = out_size + 1024;
		context->adpcm_buffer = xrealloc(context->adpcm_buffer, context->adpcm_maxlength);
	}
	dst = context->adpcm_buffer;
	while (size > 0)
	{
		if ((dst - context->adpcm_buffer) % block_size == 0)
		{
			*dst++ = context->adpcm.last_sample[0] & 0xff;
			*dst++ = (context->adpcm.last_sample[0] >> 8) & 0xff;
			*dst++ = (uint8) context->adpcm.last_step[0];
			*dst++ = 0;
			if (channels > 1)
			{
				*dst++ = context->adpcm.last_sample[1] & 0xff;
				*dst++ = (context->adpcm.last_sample[1] >> 8) & 0xff;
				*dst++ = (uint8) context->adpcm.last_step[1];
				*dst++ = 0;
			}
		}

		if (channels > 1)
		{
			memset(dst, 0, 8);
			for (i = 0; i < 16; i++)
			{
				sample = (sint16) (((uint16)(*src)) | (((uint16)(*(src + 1))) << 8));
				src += 2;
				encoded = dsp_encode_ima_adpcm_sample(&context->adpcm, i % 2, sample);
				dst[ima_stereo_encode_map[i].byte_num] |= encoded << ima_stereo_encode_map[i].byte_shift;
			}
			dst += 8;
			size -= 32;
		}
		else
		{
			sample = (sint16) (((uint16)(*src)) | (((uint16)(*(src + 1))) << 8));
			src += 2;
			encoded = dsp_encode_ima_adpcm_sample(&context->adpcm, 0, sample);
			sample = (sint16) (((uint16)(*src)) | (((uint16)(*(src + 1))) << 8));
			src += 2;
			encoded |= dsp_encode_ima_adpcm_sample(&context->adpcm, 0, sample) << 4;
			*dst++ = encoded;
			size -= 4;
		}
	}
	
	context->adpcm_size = dst - context->adpcm_buffer;
}

FREERDP_DSP_CONTEXT* freerdp_dsp_context_new(void)
{
	FREERDP_DSP_CONTEXT* context;

	context = xnew(FREERDP_DSP_CONTEXT);

	context->resample = freerdp_dsp_resample;
	context->decode_ima_adpcm = freerdp_dsp_decode_ima_adpcm;
	context->encode_ima_adpcm = freerdp_dsp_encode_ima_adpcm;

	return context;
}

void freerdp_dsp_context_free(FREERDP_DSP_CONTEXT* context)
{
	if (context)
	{
		if (context->resampled_buffer)
			xfree(context->resampled_buffer);
		if (context->adpcm_buffer)
			xfree(context->adpcm_buffer);
		xfree(context);
	}
}
