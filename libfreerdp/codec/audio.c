/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Formats
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>

#include <freerdp/log.h>
#include <freerdp/codec/audio.h>

#define TAG FREERDP_TAG("codec")

UINT32 audio_format_compute_time_length(const AUDIO_FORMAT* format, size_t size)
{
	UINT32 mstime;
	UINT32 wSamples;

	/**
	 * [MSDN-AUDIOFORMAT]:
	 * http://msdn.microsoft.com/en-us/library/ms713497.aspx
	 */

	if (format->wBitsPerSample)
	{
		wSamples = (size * 8) / format->wBitsPerSample;
		mstime = (((wSamples * 1000) / format->nSamplesPerSec) / format->nChannels);
	}
	else
	{
		mstime = 0;

		if (format->wFormatTag == WAVE_FORMAT_GSM610)
		{
			UINT16 nSamplesPerBlock;

			if ((format->cbSize == 2) && (format->data))
			{
				nSamplesPerBlock = *((UINT16*)format->data);
				wSamples = (size / format->nBlockAlign) * nSamplesPerBlock;
				mstime = (((wSamples * 1000) / format->nSamplesPerSec) / format->nChannels);
			}
			else
			{
				WLog_ERR(TAG,
				         "audio_format_compute_time_length: invalid WAVE_FORMAT_GSM610 format");
			}
		}
		else
		{
			WLog_ERR(TAG, "audio_format_compute_time_length: unknown format %" PRIu16 "",
			         format->wFormatTag);
		}
	}

	return mstime;
}

char* audio_format_get_tag_string(UINT16 wFormatTag)
{
	switch (wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			return "WAVE_FORMAT_PCM";

		case WAVE_FORMAT_ADPCM:
			return "WAVE_FORMAT_ADPCM";

		case WAVE_FORMAT_ALAW:
			return "WAVE_FORMAT_ALAW";

		case WAVE_FORMAT_MULAW:
			return "WAVE_FORMAT_MULAW";

		case WAVE_FORMAT_DVI_ADPCM:
			return "WAVE_FORMAT_DVI_ADPCM";

		case WAVE_FORMAT_GSM610:
			return "WAVE_FORMAT_GSM610";

		case WAVE_FORMAT_MSG723:
			return "WAVE_FORMAT_MSG723";

		case WAVE_FORMAT_DSPGROUP_TRUESPEECH:
			return "WAVE_FORMAT_DSPGROUP_TRUESPEECH	";

		case WAVE_FORMAT_MPEGLAYER3:
			return "WAVE_FORMAT_MPEGLAYER3";

		case WAVE_FORMAT_WMAUDIO2:
			return "WAVE_FORMAT_WMAUDIO2";

		case WAVE_FORMAT_AAC_MS:
			return "WAVE_FORMAT_AAC_MS";
	}

	return "WAVE_FORMAT_UNKNOWN";
}

void audio_format_print(wLog* log, DWORD level, const AUDIO_FORMAT* format)
{
	WLog_Print(log, level,
	           "%s:\t wFormatTag: 0x%04" PRIX16 " nChannels: %" PRIu16 " nSamplesPerSec: %" PRIu32
	           " "
	           "nAvgBytesPerSec: %" PRIu32 " nBlockAlign: %" PRIu16 " wBitsPerSample: %" PRIu16
	           " cbSize: %" PRIu16 "",
	           audio_format_get_tag_string(format->wFormatTag), format->wFormatTag,
	           format->nChannels, format->nSamplesPerSec, format->nAvgBytesPerSec,
	           format->nBlockAlign, format->wBitsPerSample, format->cbSize);
}

void audio_formats_print(wLog* log, DWORD level, const AUDIO_FORMAT* formats, UINT16 count)
{
	UINT16 index;
	const AUDIO_FORMAT* format;

	if (formats)
	{
		WLog_Print(log, level, "AUDIO_FORMATS (%" PRIu16 ") ={", count);

		for (index = 0; index < count; index++)
		{
			format = &formats[index];
			WLog_Print(log, level, "\t");
			audio_format_print(log, level, format);
		}

		WLog_Print(log, level, "}");
	}
}

BOOL audio_format_read(wStream* s, AUDIO_FORMAT* format)
{
	if (!s || !format)
		return FALSE;

	if (Stream_GetRemainingLength(s) < 18)
		return FALSE;

	Stream_Read_UINT16(s, format->wFormatTag);
	Stream_Read_UINT16(s, format->nChannels);
	Stream_Read_UINT32(s, format->nSamplesPerSec);
	Stream_Read_UINT32(s, format->nAvgBytesPerSec);
	Stream_Read_UINT16(s, format->nBlockAlign);
	Stream_Read_UINT16(s, format->wBitsPerSample);
	Stream_Read_UINT16(s, format->cbSize);

	if (Stream_GetRemainingLength(s) < format->cbSize)
		return FALSE;

	format->data = NULL;

	if (format->cbSize > 0)
	{
		format->data = malloc(format->cbSize);

		if (!format->data)
			return FALSE;

		Stream_Read(s, format->data, format->cbSize);
	}

	return TRUE;
}

BOOL audio_format_write(wStream* s, const AUDIO_FORMAT* format)
{
	if (!s || !format)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 18 + format->cbSize))
		return FALSE;

	Stream_Write_UINT16(s, format->wFormatTag);      /* wFormatTag (WAVE_FORMAT_PCM) */
	Stream_Write_UINT16(s, format->nChannels);       /* nChannels */
	Stream_Write_UINT32(s, format->nSamplesPerSec);  /* nSamplesPerSec */
	Stream_Write_UINT32(s, format->nAvgBytesPerSec); /* nAvgBytesPerSec */
	Stream_Write_UINT16(s, format->nBlockAlign);     /* nBlockAlign */
	Stream_Write_UINT16(s, format->wBitsPerSample);  /* wBitsPerSample */
	Stream_Write_UINT16(s, format->cbSize);          /* cbSize */

	if (format->cbSize > 0)
		Stream_Write(s, format->data, format->cbSize);

	return TRUE;
}

BOOL audio_format_copy(const AUDIO_FORMAT* srcFormat, AUDIO_FORMAT* dstFormat)
{
	if (!srcFormat || !dstFormat)
		return FALSE;

	*dstFormat = *srcFormat;

	if (srcFormat->cbSize > 0)
	{
		dstFormat->data = malloc(srcFormat->cbSize);

		if (!dstFormat->data)
			return FALSE;

		memcpy(dstFormat->data, srcFormat->data, dstFormat->cbSize);
	}

	return TRUE;
}

BOOL audio_format_compatible(const AUDIO_FORMAT* with, const AUDIO_FORMAT* what)
{
	if (!with || !what)
		return FALSE;

	if (with->wFormatTag != WAVE_FORMAT_UNKNOWN)
	{
		if (with->wFormatTag != what->wFormatTag)
			return FALSE;
	}

	if (with->nChannels != 0)
	{
		if (with->nChannels != what->nChannels)
			return FALSE;
	}

	if (with->nSamplesPerSec != 0)
	{
		if (with->nSamplesPerSec != what->nSamplesPerSec)
			return FALSE;
	}

	if (with->wBitsPerSample != 0)
	{
		if (with->wBitsPerSample != what->wBitsPerSample)
			return FALSE;
	}

	return TRUE;
}

static BOOL audio_format_valid(const AUDIO_FORMAT* format)
{
	if (!format)
		return FALSE;

	if (format->nChannels == 0)
		return FALSE;

	if (format->nSamplesPerSec == 0)
		return FALSE;

	return TRUE;
}

AUDIO_FORMAT* audio_format_new(void)
{
	return audio_formats_new(1);
}

AUDIO_FORMAT* audio_formats_new(size_t count)
{
	return calloc(count, sizeof(AUDIO_FORMAT));
}

void audio_format_free(AUDIO_FORMAT* format)
{
	if (format)
		free(format->data);
}

void audio_formats_free(AUDIO_FORMAT* formats, size_t count)
{
	size_t index;

	if (formats)
	{
		for (index = 0; index < count; index++)
		{
			AUDIO_FORMAT* format = &formats[index];
			audio_format_free(format);
		}

		free(formats);
	}
}
