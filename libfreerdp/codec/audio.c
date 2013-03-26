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

#include <freerdp/codec/audio.h>

UINT32 rdpsnd_compute_audio_time_length(AUDIO_FORMAT* format, int size)
{
	UINT32 mstime;
	UINT32 wSamples;

	/**
	 * [MSDN-AUDIOFORMAT]:
	 * http://msdn.microsoft.com/en-us/library/ms713497.aspx
	 */

	wSamples = (size * 8) / format->wBitsPerSample;
	mstime = (((wSamples * 1000) / format->nSamplesPerSec) / format->nChannels);

	return mstime;
}

char* rdpsnd_get_audio_tag_string(UINT16 wFormatTag)
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
	}

	return "WAVE_FORMAT_UNKNOWN";
}

void rdpsnd_print_audio_format(AUDIO_FORMAT* format)
{
	fprintf(stderr, "%s:\t wFormatTag: 0x%04X nChannels: %d nSamplesPerSec: %d nAvgBytesPerSec: %d "
			"nBlockAlign: %d wBitsPerSample: %d cbSize: %d\n",
			rdpsnd_get_audio_tag_string(format->wFormatTag), format->wFormatTag,
			format->nChannels, format->nSamplesPerSec, format->nAvgBytesPerSec,
			format->nBlockAlign, format->wBitsPerSample, format->cbSize);
}

void rdpsnd_print_audio_formats(AUDIO_FORMAT* formats, UINT16 count)
{
	int index;
	AUDIO_FORMAT* format;

	if (formats)
	{
		fprintf(stderr, "AUDIO_FORMATS (%d) =\n{\n", count);

		for (index = 0; index < (int) count; index++)
		{
			format = &formats[index];

			fprintf(stderr, "\t");
			rdpsnd_print_audio_format(format);
		}

		fprintf(stderr, "}\n");
	}
}

void rdpsnd_free_audio_formats(AUDIO_FORMAT* formats, UINT16 count)
{
	int index;
	AUDIO_FORMAT* format;

	if (formats)
	{
		for (index = 0; index < (int) count; index++)
		{
			format = &formats[index];

			if (format->cbSize)
				free(format->data);
		}

		free(formats);
	}
}
