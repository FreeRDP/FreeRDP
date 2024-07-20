/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Server Common
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/wtypes.h>
#include <freerdp/codec/audio.h>
#include <freerdp/codec/dsp.h>

#include <freerdp/server/server-common.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("server.common")

size_t server_audin_get_formats(AUDIO_FORMAT** dst_formats)
{
	/* Default supported audio formats */
	BYTE adpcm_data_7[] = { 0xf4, 0x07, 0x07, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00,
		                    0xff, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x40, 0x00, 0xf0, 0x00,
		                    0x00, 0x00, 0xcc, 0x01, 0x30, 0xff, 0x88, 0x01, 0x18, 0xff };
	BYTE adpcm_data_3[] = { 0xf4, 0x03, 0x07, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00,
		                    0xff, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x40, 0x00, 0xf0, 0x00,
		                    0x00, 0x00, 0xcc, 0x01, 0x30, 0xff, 0x88, 0x01, 0x18, 0xff };
	BYTE adpcm_data_1[] = { 0xf4, 0x01, 0x07, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00,
		                    0xff, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x40, 0x00, 0xf0, 0x00,
		                    0x00, 0x00, 0xcc, 0x01, 0x30, 0xff, 0x88, 0x01, 0x18, 0xff };
	BYTE adpcm_dvi_data_7[] = { 0xf9, 0x07 };
	BYTE adpcm_dvi_data_3[] = { 0xf9, 0x03 };
	BYTE adpcm_dvi_data_1[] = { 0xf9, 0x01 };
	BYTE gsm610_data[] = { 0x40, 0x01 };
	const AUDIO_FORMAT default_supported_audio_formats[] = {
		/* Formats sent by windows 10 server */
		{ WAVE_FORMAT_AAC_MS, 2, 44100, 24000, 4, 16, 0, NULL },
		{ WAVE_FORMAT_AAC_MS, 2, 44100, 20000, 4, 16, 0, NULL },
		{ WAVE_FORMAT_AAC_MS, 2, 44100, 16000, 4, 16, 0, NULL },
		{ WAVE_FORMAT_AAC_MS, 2, 44100, 12000, 4, 16, 0, NULL },
		{ WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0, NULL },
		{ WAVE_FORMAT_ADPCM, 2, 44100, 44359, 2048, 4, 32, adpcm_data_7 },
		{ WAVE_FORMAT_DVI_ADPCM, 2, 44100, 44251, 2048, 4, 2, adpcm_dvi_data_7 },
		{ WAVE_FORMAT_ALAW, 2, 22050, 44100, 2, 8, 0, NULL },
		{ WAVE_FORMAT_ADPCM, 2, 22050, 22311, 1024, 4, 32, adpcm_data_3 },
		{ WAVE_FORMAT_DVI_ADPCM, 2, 22050, 22201, 1024, 4, 2, adpcm_dvi_data_3 },
		{ WAVE_FORMAT_ADPCM, 1, 44100, 22179, 1024, 4, 32, adpcm_data_7 },
		{ WAVE_FORMAT_DVI_ADPCM, 1, 44100, 22125, 1024, 4, 2, adpcm_dvi_data_7 },
		{ WAVE_FORMAT_ADPCM, 2, 11025, 11289, 512, 4, 32, adpcm_data_1 },
		{ WAVE_FORMAT_DVI_ADPCM, 2, 11025, 11177, 512, 4, 2, adpcm_dvi_data_1 },
		{ WAVE_FORMAT_ADPCM, 1, 22050, 11155, 512, 4, 32, adpcm_data_3 },
		{ WAVE_FORMAT_DVI_ADPCM, 1, 22050, 11100, 512, 4, 2, adpcm_dvi_data_3 },
		{ WAVE_FORMAT_GSM610, 1, 44100, 8957, 65, 0, 2, gsm610_data },
		{ WAVE_FORMAT_ADPCM, 2, 8000, 8192, 512, 4, 32, adpcm_data_1 },
		{ WAVE_FORMAT_DVI_ADPCM, 2, 8000, 8110, 512, 4, 2, adpcm_dvi_data_1 },
		{ WAVE_FORMAT_ADPCM, 1, 11025, 5644, 256, 4, 32, adpcm_data_1 },
		{ WAVE_FORMAT_DVI_ADPCM, 1, 11025, 5588, 256, 4, 2, adpcm_dvi_data_1 },
		{ WAVE_FORMAT_GSM610, 1, 22050, 4478, 65, 0, 2, gsm610_data },
		{ WAVE_FORMAT_ADPCM, 1, 8000, 4096, 256, 4, 32, adpcm_data_1 },
		{ WAVE_FORMAT_DVI_ADPCM, 1, 8000, 4055, 256, 4, 2, adpcm_dvi_data_1 },
		{ WAVE_FORMAT_GSM610, 1, 11025, 2239, 65, 0, 2, gsm610_data },
		{ WAVE_FORMAT_GSM610, 1, 8000, 1625, 65, 0, 2, gsm610_data },
		/* Formats added for others */

		{ WAVE_FORMAT_MSG723, 2, 44100, 0, 4, 16, 0, NULL },
		{ WAVE_FORMAT_MSG723, 2, 22050, 0, 4, 16, 0, NULL },
		{ WAVE_FORMAT_MSG723, 1, 44100, 0, 4, 16, 0, NULL },
		{ WAVE_FORMAT_MSG723, 1, 22050, 0, 4, 16, 0, NULL },
		{ WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0, NULL },
		{ WAVE_FORMAT_PCM, 2, 22050, 88200, 4, 16, 0, NULL },
		{ WAVE_FORMAT_PCM, 1, 44100, 88200, 4, 16, 0, NULL },
		{ WAVE_FORMAT_PCM, 1, 22050, 44100, 4, 16, 0, NULL },
		{ WAVE_FORMAT_MULAW, 2, 44100, 88200, 4, 16, 0, NULL },
		{ WAVE_FORMAT_MULAW, 2, 22050, 44100, 4, 16, 0, NULL },
		{ WAVE_FORMAT_MULAW, 1, 44100, 44100, 4, 16, 0, NULL },
		{ WAVE_FORMAT_MULAW, 1, 22050, 22050, 4, 16, 0, NULL },
		{ WAVE_FORMAT_ALAW, 2, 44100, 88200, 2, 8, 0, NULL },
		{ WAVE_FORMAT_ALAW, 2, 22050, 44100, 2, 8, 0, NULL },
		{ WAVE_FORMAT_ALAW, 1, 44100, 44100, 2, 8, 0, NULL },
		{ WAVE_FORMAT_ALAW, 1, 22050, 22050, 2, 8, 0, NULL }
	};
	const size_t nrDefaultFormatsMax = ARRAYSIZE(default_supported_audio_formats);
	size_t nr_formats = 0;
	AUDIO_FORMAT* formats = audio_formats_new(nrDefaultFormatsMax);

	if (!dst_formats)
		goto fail;

	*dst_formats = NULL;

	if (!formats)
		goto fail;

	for (size_t x = 0; x < nrDefaultFormatsMax; x++)
	{
		const AUDIO_FORMAT* format = &default_supported_audio_formats[x];

		if (freerdp_dsp_supports_format(format, FALSE))
		{
			AUDIO_FORMAT* dst = &formats[nr_formats++];

			if (!audio_format_copy(format, dst))
				goto fail;
		}
	}

	*dst_formats = formats;
	return nr_formats;
fail:
	audio_formats_free(formats, nrDefaultFormatsMax);
	return 0;
}

size_t server_rdpsnd_get_formats(AUDIO_FORMAT** dst_formats)
{
	/* Default supported audio formats */
	static const AUDIO_FORMAT default_supported_audio_formats[] = {
		{ WAVE_FORMAT_AAC_MS, 2, 44100, 176400, 4, 16, 0, NULL },
		{ WAVE_FORMAT_MPEGLAYER3, 2, 44100, 176400, 4, 16, 0, NULL },
		{ WAVE_FORMAT_MSG723, 2, 44100, 176400, 4, 16, 0, NULL },
		{ WAVE_FORMAT_GSM610, 2, 44100, 176400, 4, 16, 0, NULL },
		{ WAVE_FORMAT_ADPCM, 2, 44100, 176400, 4, 16, 0, NULL },
		{ WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0, NULL },
		{ WAVE_FORMAT_ALAW, 2, 22050, 44100, 2, 8, 0, NULL },
		{ WAVE_FORMAT_MULAW, 2, 22050, 44100, 2, 8, 0, NULL },
	};
	AUDIO_FORMAT* supported_audio_formats =
	    audio_formats_new(ARRAYSIZE(default_supported_audio_formats));

	if (!supported_audio_formats)
		goto fail;

	size_t y = 0;
	for (size_t x = 0; x < ARRAYSIZE(default_supported_audio_formats); x++)
	{
		const AUDIO_FORMAT* format = &default_supported_audio_formats[x];

		if (freerdp_dsp_supports_format(format, TRUE))
			supported_audio_formats[y++] = *format;
	}

	/* Set default audio formats. */
	*dst_formats = supported_audio_formats;
	return y;
fail:
	audio_formats_free(supported_audio_formats, ARRAYSIZE(default_supported_audio_formats));

	if (dst_formats)
		*dst_formats = NULL;

	return 0;
}

void freerdp_server_warn_unmaintained(int argc, char* argv[])
{
	const char* app = (argc > 0) ? argv[0] : "INVALID_ARGV";
	const DWORD log_level = WLOG_WARN;
	wLog* log = WLog_Get(TAG);
	WINPR_ASSERT(log);

	if (!WLog_IsLevelActive(log, log_level))
		return;

	WLog_Print_unchecked(log, log_level, "[unmaintained] %s server is currently unmaintained!",
	                     app);
	WLog_Print_unchecked(
	    log, log_level,
	    " If problems occur please check https://github.com/FreeRDP/FreeRDP/issues for "
	    "known issues!");
	WLog_Print_unchecked(
	    log, log_level,
	    "Be prepared to fix issues yourself though as nobody is actively working on this.");
	WLog_Print_unchecked(
	    log, log_level,
	    " Developers hang out in https://matrix.to/#/#FreeRDP:matrix.org?via=matrix.org "
	    "- dont hesitate to ask some questions. (replies might take some time depending "
	    "on your timezone) - if you intend using this component write us a message");
}

void freerdp_server_warn_experimental(int argc, char* argv[])
{
	const char* app = (argc > 0) ? argv[0] : "INVALID_ARGV";
	const DWORD log_level = WLOG_WARN;
	wLog* log = WLog_Get(TAG);
	WINPR_ASSERT(log);

	if (!WLog_IsLevelActive(log, log_level))
		return;

	WLog_Print_unchecked(log, log_level, "[experimental] %s server is currently experimental!",
	                     app);
	WLog_Print_unchecked(
	    log, log_level,
	    " If problems occur please check https://github.com/FreeRDP/FreeRDP/issues for "
	    "known issues or create a new one!");
	WLog_Print_unchecked(
	    log, log_level,
	    " Developers hang out in https://matrix.to/#/#FreeRDP:matrix.org?via=matrix.org "
	    "- dont hesitate to ask some questions. (replies might take some time depending "
	    "on your timezone)");
}

void freerdp_server_warn_deprecated(int argc, char* argv[])
{
	const char* app = (argc > 0) ? argv[0] : "INVALID_ARGV";
	const DWORD log_level = WLOG_WARN;
	wLog* log = WLog_Get(TAG);
	WINPR_ASSERT(log);

	if (!WLog_IsLevelActive(log, log_level))
		return;

	WLog_Print_unchecked(log, log_level, "[deprecated] %s server has been deprecated", app);
	WLog_Print_unchecked(log, log_level, "As replacement there is a SDL based client available.");
	WLog_Print_unchecked(
	    log, log_level,
	    "If you are interested in keeping %s alive get in touch with the developers", app);
	WLog_Print_unchecked(
	    log, log_level,
	    "The project is hosted at https://github.com/freerdp/freerdp and "
	    " developers hang out in https://matrix.to/#/#FreeRDP:matrix.org?via=matrix.org "
	    "- dont hesitate to ask some questions. (replies might take some time depending "
	    "on your timezone)");
}
