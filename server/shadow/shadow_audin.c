/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2015 Jiang Zihao <zihao.jiang@yahoo.com>
 * Copyright 2023 Pascal Nowack <Pascal.Nowack@gmx.de>
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
#include "shadow.h"

#include "shadow_audin.h"
#include <freerdp/server/server-common.h>

#if defined(CHANNEL_AUDIN_SERVER)
#include <freerdp/server/audin.h>
#endif

#define TAG SERVER_TAG("shadow")

#if defined(CHANNEL_AUDIN_SERVER)

static UINT AudinServerReceiveVersion(audin_server_context* audin, const SNDIN_VERSION* version)
{
	rdpShadowClient* client;
	SNDIN_FORMATS formats = { 0 };

	WINPR_ASSERT(audin);
	WINPR_ASSERT(version);

	client = audin->userdata;
	WINPR_ASSERT(client);

	if (version->Version == 0)
	{
		WLog_ERR(TAG, "Received invalid AUDIO_INPUT version from client");
		return ERROR_INVALID_DATA;
	}

	WLog_DBG(TAG, "AUDIO_INPUT version of client: %u", version->Version);

	formats.NumFormats = client->audin_n_server_formats;
	formats.SoundFormats = client->audin_server_formats;

	return audin->SendFormats(audin, &formats);
}

static UINT send_open(audin_server_context* audin)
{
	rdpShadowClient* client = audin->userdata;
	SNDIN_OPEN open = { 0 };

	WINPR_ASSERT(audin);

	client = audin->userdata;
	WINPR_ASSERT(client);

	open.FramesPerPacket = 441;
	open.initialFormat = client->audin_client_format_idx;
	open.captureFormat.wFormatTag = WAVE_FORMAT_PCM;
	open.captureFormat.nChannels = 2;
	open.captureFormat.nSamplesPerSec = 44100;
	open.captureFormat.nAvgBytesPerSec = 44100 * 2 * 2;
	open.captureFormat.nBlockAlign = 4;
	open.captureFormat.wBitsPerSample = 16;

	return audin->SendOpen(audin, &open);
}

static UINT AudinServerReceiveFormats(audin_server_context* audin, const SNDIN_FORMATS* formats)
{
	rdpShadowClient* client;

	WINPR_ASSERT(audin);
	WINPR_ASSERT(formats);

	client = audin->userdata;
	WINPR_ASSERT(client);

	if (client->audin_negotiated_format)
	{
		WLog_ERR(TAG, "Received client formats, but negotiation was already done");
		return ERROR_INVALID_DATA;
	}

	for (size_t i = 0; i < client->audin_n_server_formats; ++i)
	{
		for (UINT32 j = 0; j < formats->NumFormats; ++j)
		{
			if (audio_format_compatible(&client->audin_server_formats[i],
			                            &formats->SoundFormats[j]))
			{
				client->audin_negotiated_format = &client->audin_server_formats[i];
				client->audin_client_format_idx = i;
				return send_open(audin);
			}
		}
	}

	WLog_ERR(TAG, "Could not agree on a audio format with the server");

	return ERROR_INVALID_DATA;
}

static UINT AudinServerOpenReply(audin_server_context* audin, const SNDIN_OPEN_REPLY* open_reply)
{
	WINPR_ASSERT(audin);
	WINPR_ASSERT(open_reply);

	/* TODO: Implement failure handling */
	WLog_DBG(TAG, "Open Reply PDU: Result: %i", open_reply->Result);
	return CHANNEL_RC_OK;
}

static UINT AudinServerIncomingData(audin_server_context* audin,
                                    const SNDIN_DATA_INCOMING* data_incoming)
{
	WINPR_ASSERT(audin);
	WINPR_ASSERT(data_incoming);

	/* TODO: Implement bandwidth measure of clients uplink */
	WLog_DBG(TAG, "Received Incoming Data PDU");
	return CHANNEL_RC_OK;
}

static UINT AudinServerData(audin_server_context* audin, const SNDIN_DATA* data)
{
	rdpShadowClient* client;
	rdpShadowSubsystem* subsystem;

	WINPR_ASSERT(audin);
	WINPR_ASSERT(data);

	client = audin->userdata;
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->server);
	subsystem = client->server->subsystem;
	WINPR_ASSERT(subsystem);

	if (!client->mayInteract)
		return CHANNEL_RC_OK;

	if (!IFCALLRESULT(TRUE, subsystem->AudinServerReceiveSamples, subsystem, client,
	                  client->audin_negotiated_format, data->Data))
		return ERROR_INTERNAL_ERROR;

	return CHANNEL_RC_OK;
}

static UINT AudinServerReceiveFormatChange(audin_server_context* audin,
                                           const SNDIN_FORMATCHANGE* format_change)
{
	rdpShadowClient* client;

	WINPR_ASSERT(audin);
	WINPR_ASSERT(format_change);

	client = audin->userdata;
	WINPR_ASSERT(client);

	if (format_change->NewFormat != client->audin_client_format_idx)
	{
		WLog_ERR(TAG, "NewFormat in FormatChange differs from requested format");
		return ERROR_INVALID_DATA;
	}

	WLog_DBG(TAG, "Received Format Change PDU: %u", format_change->NewFormat);

	return CHANNEL_RC_OK;
}
#endif

BOOL shadow_client_audin_init(rdpShadowClient* client)
{
	WINPR_ASSERT(client);

#if defined(CHANNEL_AUDIN_SERVER)
	audin_server_context* audin;
	audin = client->audin = audin_server_context_new(client->vcm);

	if (!audin)
		return FALSE;

	audin->userdata = client;

	audin->ReceiveVersion = AudinServerReceiveVersion;
	audin->ReceiveFormats = AudinServerReceiveFormats;
	audin->OpenReply = AudinServerOpenReply;
	audin->IncomingData = AudinServerIncomingData;
	audin->Data = AudinServerData;
	audin->ReceiveFormatChange = AudinServerReceiveFormatChange;

	if (client->subsystem->audinFormats)
	{
		size_t x;
		client->audin_server_formats = audio_formats_new(client->subsystem->nAudinFormats);

		if (!client->audin_server_formats)
			goto fail;

		for (x = 0; x < client->subsystem->nAudinFormats; x++)
		{
			if (!audio_format_copy(&client->subsystem->audinFormats[x],
			                       &client->audin_server_formats[x]))
				goto fail;
		}

		client->audin_n_server_formats = client->subsystem->nAudinFormats;
	}
	else
	{
		client->audin_n_server_formats = server_audin_get_formats(&client->audin_server_formats);
	}

	if (client->audin_n_server_formats < 1)
		goto fail;

	client->audin_negotiated_format = NULL;

	return TRUE;
fail:
	audin_server_context_free(audin);
	client->audin = NULL;
#endif
	return FALSE;
}

void shadow_client_audin_uninit(rdpShadowClient* client)
{
	WINPR_ASSERT(client);

#if defined(CHANNEL_AUDIN_SERVER)
	if (client->audin)
	{
		audio_formats_free(client->audin_server_formats, client->audin_n_server_formats);
		client->audin_server_formats = NULL;
		audin_server_context_free(client->audin);
		client->audin = NULL;
	}
#endif
}
