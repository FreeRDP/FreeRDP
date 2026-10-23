/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <freerdp/channels/log.h>
#include <freerdp/channels/drdynvc.h>

#include "drdynvc_main.h"

#define TAG CHANNELS_TAG("drdynvc.server")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_server_start(DrdynvcServerContext* context)
{
	context->priv->ChannelHandle =
	    WTSVirtualChannelOpen(context->vcm, WTS_CURRENT_SESSION, DRDYNVC_SVC_CHANNEL_NAME);

	if (!context->priv->ChannelHandle)
	{
		WLog_ERR(TAG, "WTSVirtualChannelOpen failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!(context->priv->StopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_server_stop(DrdynvcServerContext* context)
{
	UINT error = 0;
	(void)SetEvent(context->priv->StopEvent);

	if (WaitForSingleObject(context->priv->Thread, INFINITE) == WAIT_FAILED)
	{
		error = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "!", error);
		return error;
	}

	(void)CloseHandle(context->priv->Thread);
	return CHANNEL_RC_OK;
}

DrdynvcServerContext* drdynvc_server_context_new(HANDLE vcm)
{
	DrdynvcServerContext* context = nullptr;
	context = (DrdynvcServerContext*)calloc(1, sizeof(DrdynvcServerContext));

	if (context)
	{
		context->vcm = vcm;
		context->Start = drdynvc_server_start;
		context->Stop = drdynvc_server_stop;
		context->priv = (DrdynvcServerPrivate*)calloc(1, sizeof(DrdynvcServerPrivate));

		if (!context->priv)
		{
			WLog_ERR(TAG, "calloc failed!");
			free(context);
			return nullptr;
		}
	}
	else
	{
		WLog_ERR(TAG, "calloc failed!");
	}

	return context;
}

void drdynvc_server_context_free(DrdynvcServerContext* context)
{
	if (context)
	{
		free(context->priv);
		free(context);
	}
}
