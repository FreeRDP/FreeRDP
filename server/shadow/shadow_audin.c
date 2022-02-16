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

#include <freerdp/config.h>

#include <freerdp/log.h>
#include <freerdp/codec/dsp.h>
#include "shadow.h"

#include "shadow_audin.h"
#include <freerdp/server/server-common.h>

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

	return IFCALLRESULT(ERROR_CALL_NOT_IMPLEMENTED, context->SelectFormat, context, i);
}
/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT AudinServerOpenResult(audin_server_context* context, UINT32 result)
{
	/* TODO: Implement */
	WLog_WARN(TAG, "%s not implemented", __FUNCTION__);
	WLog_INFO(TAG, "AUDIN open result %" PRIu32 ".\n", result);
	return CHANNEL_RC_OK;
}
/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT AudinServerReceiveSamples(audin_server_context* context, const AUDIO_FORMAT* format,
                                      wStream* buf, size_t nframes)
{
	rdpShadowClient* client = (rdpShadowClient*)context->data;
	rdpShadowSubsystem* subsystem = client->server->subsystem;

	if (!client->mayInteract)
		return CHANNEL_RC_OK;

	if (!IFCALLRESULT(TRUE, subsystem->AudinServerReceiveSamples, subsystem, client, format, buf,
	                  nframes))
		return ERROR_INTERNAL_ERROR;

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
		audin->num_server_formats = server_audin_get_formats(&audin->server_formats);
	}

	if (audin->num_server_formats < 1)
		goto fail;

	audin->dst_format = &audin->server_formats[0];
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
