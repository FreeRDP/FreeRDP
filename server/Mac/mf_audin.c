/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Mac OS X Server (Audio Input)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include "mfreerdp.h"

#include "mf_audin.h"

#include <freerdp/server/server-common.h>
#include <freerdp/log.h>
#define TAG SERVER_TAG("mac")

static UINT mf_peer_audin_receive_version(audin_server_context* audin, const SNDIN_VERSION* version)
{
	mfPeerContext* context;
	SNDIN_FORMATS formats = { 0 };

	WINPR_ASSERT(audin);
	WINPR_ASSERT(version);

	context = audin->userdata;
	WINPR_ASSERT(context);

	if (version->Version == 0)
	{
		WLog_ERR(TAG, "Received invalid AUDIO_INPUT version from client");
		return ERROR_INVALID_DATA;
	}

	WLog_DBG(TAG, "AUDIO_INPUT version of client: %u", version->Version);

	formats.NumFormats = context->audin_n_server_formats;
	formats.SoundFormats = context->audin_server_formats;

	return audin->SendFormats(audin, &formats);
}

static UINT send_open(audin_server_context* audin)
{
	mfPeerContext* context;
	SNDIN_OPEN open = { 0 };

	WINPR_ASSERT(audin);

	context = audin->userdata;
	WINPR_ASSERT(context);

	open.FramesPerPacket = 441;
	open.initialFormat = context->audin_client_format_idx;
	open.captureFormat.wFormatTag = WAVE_FORMAT_PCM;
	open.captureFormat.nChannels = 2;
	open.captureFormat.nSamplesPerSec = 44100;
	open.captureFormat.nAvgBytesPerSec = 44100 * 2 * 2;
	open.captureFormat.nBlockAlign = 4;
	open.captureFormat.wBitsPerSample = 16;

	return audin->SendOpen(audin, &open);
}

static UINT mf_peer_audin_receive_formats(audin_server_context* audin, const SNDIN_FORMATS* formats)
{
	mfPeerContext* context;

	WINPR_ASSERT(audin);
	WINPR_ASSERT(formats);

	context = audin->userdata;
	WINPR_ASSERT(context);

	if (context->audin_negotiated_format)
	{
		WLog_ERR(TAG, "Received client formats, but negotiation was already done");
		return ERROR_INVALID_DATA;
	}

	for (size_t i = 0; i < context->audin_n_server_formats; ++i)
	{
		for (UINT32 j = 0; j < formats->NumFormats; ++j)
		{
			if (audio_format_compatible(&context->audin_server_formats[i],
			                            &formats->SoundFormats[j]))
			{
				context->audin_negotiated_format = &context->audin_server_formats[i];
				context->audin_client_format_idx = i;
				return send_open(audin);
			}
		}
	}

	WLog_ERR(TAG, "Could not agree on a audio format with the server");

	return ERROR_INVALID_DATA;
}

static UINT mf_peer_audin_open_reply(audin_server_context* audin,
                                     const SNDIN_OPEN_REPLY* open_reply)
{
	WINPR_ASSERT(audin);
	WINPR_ASSERT(open_reply);

	/* TODO: Implement failure handling */
	WLog_DBG(TAG, "Open Reply PDU: Result: %i", open_reply->Result);
	return CHANNEL_RC_OK;
}

static UINT mf_peer_audin_incoming_data(audin_server_context* audin,
                                        const SNDIN_DATA_INCOMING* data_incoming)
{
	WINPR_ASSERT(audin);
	WINPR_ASSERT(data_incoming);

	/* TODO: Implement bandwidth measure of clients uplink */
	WLog_DBG(TAG, "Received Incoming Data PDU");
	return CHANNEL_RC_OK;
}

static UINT mf_peer_audin_data(audin_server_context* audin, const SNDIN_DATA* data)
{
	/* TODO: Implement */
	WINPR_ASSERT(audin);
	WINPR_ASSERT(data);

	WLog_WARN(TAG, "not implemented");
	WLog_DBG(TAG, "receive %" PRIdz " bytes.", Stream_Length(data->Data));
	return CHANNEL_RC_OK;
}

static UINT mf_peer_audin_receive_format_change(audin_server_context* audin,
                                                const SNDIN_FORMATCHANGE* format_change)
{
	mfPeerContext* context;

	WINPR_ASSERT(audin);
	WINPR_ASSERT(format_change);

	context = audin->userdata;
	WINPR_ASSERT(context);

	if (format_change->NewFormat != context->audin_client_format_idx)
	{
		WLog_ERR(TAG, "NewFormat in FormatChange differs from requested format");
		return ERROR_INVALID_DATA;
	}

	WLog_DBG(TAG, "Received Format Change PDU: %u", format_change->NewFormat);

	return CHANNEL_RC_OK;
}

void mf_peer_audin_init(mfPeerContext* context)
{
	WINPR_ASSERT(context);

	context->audin = audin_server_context_new(context->vcm);
	context->audin->rdpcontext = &context->_p;
	context->audin->userdata = context;

	context->audin->ReceiveVersion = mf_peer_audin_receive_version;
	context->audin->ReceiveFormats = mf_peer_audin_receive_formats;
	context->audin->OpenReply = mf_peer_audin_open_reply;
	context->audin->IncomingData = mf_peer_audin_incoming_data;
	context->audin->Data = mf_peer_audin_data;
	context->audin->ReceiveFormatChange = mf_peer_audin_receive_format_change;

	context->audin_n_server_formats = server_audin_get_formats(&context->audin_server_formats);
}

void mf_peer_audin_uninit(mfPeerContext* context)
{
	WINPR_ASSERT(context);

	if (context->audin)
	{
		audio_formats_free(context->audin_server_formats, context->audin_n_server_formats);
		context->audin_server_formats = NULL;
		audin_server_context_free(context->audin);
		context->audin = NULL;
	}
}
