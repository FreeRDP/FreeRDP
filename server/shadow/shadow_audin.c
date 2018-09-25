/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2015 Jiang Zihao <zihao.jiang@yahoo.com>
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
#include <freerdp/codec/dsp.h>
#include "shadow.h"

#include "shadow_audin.h"

#define TAG SERVER_TAG("shadow")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT AudinServerOpening(audin_server_context* context)
{
	AUDIO_FORMAT* agreed_format = NULL;
	size_t i = 0, j = 0;

	for (i = 0; i < context->num_client_formats; i++)
	{
		for (j = 0; j < context->num_server_formats; j++)
		{
			if (audio_format_compatible(&context->server_formats[j], &context->client_formats[i]))
			{
				agreed_format = &context->server_formats[j];
				break;
			}
		}

		if (agreed_format != NULL)
			break;
	}

	if (agreed_format == NULL)
	{
		WLog_ERR(TAG, "Could not agree on a audio format with the server\n");
		return CHANNEL_RC_OK;
	}

	context->SelectFormat(context, i);
	return CHANNEL_RC_OK;
}
/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT AudinServerOpenResult(audin_server_context* context, UINT32 result)
{
	WLog_INFO(TAG, "AUDIN open result %"PRIu32".\n", result);
	return CHANNEL_RC_OK;
}
/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT AudinServerReceiveSamples(audin_server_context* context, const void* buf, int nframes)
{
	rdpShadowClient* client = (rdpShadowClient*)context->data;
	rdpShadowSubsystem* subsystem = client->server->subsystem;

	if (!client->mayInteract)
		return CHANNEL_RC_OK;

	if (subsystem->AudinServerReceiveSamples)
		subsystem->AudinServerReceiveSamples(subsystem, client, buf, nframes);

	return CHANNEL_RC_OK;
}

BOOL shadow_client_audin_init(rdpShadowClient* client)
{
	audin_server_context* audin;
	audin = client->audin = audin_server_context_new(client->vcm);

	if (!audin)
		return FALSE;

	audin->data = client;

	if (client->subsystem->audinFormats)
	{
		size_t x;
		audin->server_formats = audio_formats_new(client->subsystem->nAudinFormats);

		if (!audin->server_formats)
			goto fail;

		for (x = 0; x < client->subsystem->nAudinFormats; x++)
		{
			if (!audio_format_copy(&client->subsystem->audinFormats[x], &audin->server_formats[x]))
				goto fail;
		}

		audin->num_server_formats = client->subsystem->nAudinFormats;
	}
	else
	{
		/* Default supported audio formats */
		BYTE adpcm_data_7[] =
		{
			0xf4, 0x07, 0x07, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00,
			0xff, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x40, 0x00, 0xf0, 0x00,
			0x00, 0x00, 0xcc, 0x01, 0x30, 0xff, 0x88, 0x01, 0x18, 0xff
		};
		BYTE adpcm_data_3[] =
		{
			0xf4, 0x03, 0x07, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00,
			0xff, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x40, 0x00, 0xf0, 0x00,
			0x00, 0x00, 0xcc, 0x01, 0x30, 0xff, 0x88, 0x01, 0x18, 0xff
		};
		BYTE adpcm_data_1[] =
		{
			0xf4, 0x01, 0x07, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00,
			0xff, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x40, 0x00, 0xf0, 0x00,
			0x00, 0x00, 0xcc, 0x01, 0x30, 0xff, 0x88, 0x01, 0x18, 0xff
		};
		BYTE adpcm_dvi_data_7[] = { 0xf9, 0x07 };
		BYTE adpcm_dvi_data_3[] = { 0xf9, 0x03 };
		BYTE adpcm_dvi_data_1[] = { 0xf9, 0x01 };
		BYTE gsm610_data[] = { 0x40, 0x01 };
		const AUDIO_FORMAT default_supported_audio_formats[] =
		{
			/* Formats sent by windows 10 server */
			{ WAVE_FORMAT_AAC_MS,    2, 44100,  24000,    4, 16,  0, NULL },
			{ WAVE_FORMAT_AAC_MS,    2, 44100,  20000,    4, 16,  0, NULL },
			{ WAVE_FORMAT_AAC_MS,    2, 44100,  16000,    4, 16,  0, NULL },
			{ WAVE_FORMAT_AAC_MS,    2, 44100,  12000,    4, 16,  0, NULL },
			{ WAVE_FORMAT_PCM,       2, 44100, 176400,    4, 16,  0, NULL },
			{ WAVE_FORMAT_ADPCM,     2, 44100,  44359, 2048,  4, 32, adpcm_data_7 },
			{ WAVE_FORMAT_DVI_ADPCM, 2, 44100,  44251, 2048,  4,  2, adpcm_dvi_data_7 },
			{ WAVE_FORMAT_ALAW,      2, 22050,  44100,    2,  8,  0, NULL },
			{ WAVE_FORMAT_ADPCM,     2, 22050,  22311, 1024,  4, 32, adpcm_data_3 },
			{ WAVE_FORMAT_DVI_ADPCM, 2, 22050,  22201, 1024,  4,  2, adpcm_dvi_data_3 },
			{ WAVE_FORMAT_ADPCM,     1, 44100,  22179, 1024,  4, 32, adpcm_data_7 },
			{ WAVE_FORMAT_DVI_ADPCM, 1, 44100,  22125, 1024,  4,  2, adpcm_dvi_data_7 },
			{ WAVE_FORMAT_ADPCM,     2, 11025,  11289,  512,  4, 32, adpcm_data_1 },
			{ WAVE_FORMAT_DVI_ADPCM, 2, 11025,  11177,  512,  4,  2, adpcm_dvi_data_1 },
			{ WAVE_FORMAT_ADPCM,     1, 22050,  11155,  512,  4, 32, adpcm_data_3 },
			{ WAVE_FORMAT_DVI_ADPCM, 1, 22050,  11100,  512,  4,  2, adpcm_dvi_data_3 },
			{ WAVE_FORMAT_GSM610,    1, 44100,   8957,   65,  0,  2, gsm610_data },
			{ WAVE_FORMAT_ADPCM,     2,  8000,   8192,  512,  4, 32, adpcm_data_1 },
			{ WAVE_FORMAT_DVI_ADPCM, 2,  8000,   8110,  512,  4,  2, adpcm_dvi_data_1 },
			{ WAVE_FORMAT_ADPCM,     1, 11025,   5644,  256,  4, 32, adpcm_data_1 },
			{ WAVE_FORMAT_DVI_ADPCM, 1, 11025,   5588,  256,  4,  2, adpcm_dvi_data_1 },
			{ WAVE_FORMAT_GSM610,    1, 22050,   4478,   65,  0,  2, gsm610_data },
			{ WAVE_FORMAT_ADPCM,     1,  8000,   4096,  256,  4, 32, adpcm_data_1 },
			{ WAVE_FORMAT_DVI_ADPCM, 1,  8000,   4055,  256,  4,  2, adpcm_dvi_data_1 },
			{ WAVE_FORMAT_GSM610,    1, 11025,   2239,   65,  0,  2, gsm610_data },
			{ WAVE_FORMAT_GSM610,    1,  8000,   1625,   65,  0,  2, gsm610_data },
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
		size_t x;
		audin->server_formats = audio_formats_new(nrDefaultFormatsMax);

		if (!audin->server_formats)
			goto fail;

		/* Set default audio formats. */
		for (x = 0; x < nrDefaultFormatsMax; x++)
		{
			const AUDIO_FORMAT* format = &default_supported_audio_formats[x];

			if (freerdp_dsp_supports_format(format, FALSE))
			{
				AUDIO_FORMAT* dst = &audin->server_formats[audin->num_server_formats++];
				audio_format_copy(format, dst);
			}
		}
	}

	if (audin->num_server_formats < 1)
		goto fail;

	audin->dst_format = audin->server_formats[0];
	audin->Opening = AudinServerOpening;
	audin->OpenResult = AudinServerOpenResult;
	audin->ReceiveSamples = AudinServerReceiveSamples;
	return TRUE;
fail:
	audin_server_context_free(audin);
	client->audin = NULL;
	return FALSE;
}

void shadow_client_audin_uninit(rdpShadowClient* client)
{
	if (client->audin)
	{
		audin_server_context_free(client->audin);
		client->audin = NULL;
	}
}
