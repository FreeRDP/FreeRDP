/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Echo Virtual Channel Extension
 *
 * Copyright 2013 Christian Hofstaedtler
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

#include <stdio.h>
#include <stdlib.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/stream.h>

#include "echo_main.h"
#include <freerdp/client/channels.h>
#include <freerdp/channels/log.h>
#include <freerdp/channels/echo.h>

#define TAG CHANNELS_TAG("echo.client")

typedef struct
{
	GENERIC_DYNVC_PLUGIN baseDynPlugin;
} ECHO_PLUGIN;

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT echo_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	const BYTE* pBuffer = Stream_ConstPointer(data);
	const size_t cbSize = Stream_GetRemainingLength(data);

	WINPR_ASSERT(callback);
	WINPR_ASSERT(callback->channel);
	WINPR_ASSERT(callback->channel->Write);

	if (cbSize > UINT32_MAX)
		return ERROR_INVALID_PARAMETER;

	/* echo back what we have received. ECHO does not have any message IDs. */
	return callback->channel->Write(callback->channel, (ULONG)cbSize, pBuffer, NULL);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT echo_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;

	free(callback);

	return CHANNEL_RC_OK;
}

static const IWTSVirtualChannelCallback echo_callbacks = { echo_on_data_received, NULL, /* Open */
	                                                       echo_on_close, NULL };

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
FREERDP_ENTRY_POINT(UINT echo_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints))
{
	return freerdp_generic_DVCPluginEntry(pEntryPoints, TAG, ECHO_DVC_CHANNEL_NAME,
	                                      sizeof(ECHO_PLUGIN), sizeof(GENERIC_CHANNEL_CALLBACK),
	                                      &echo_callbacks, NULL, NULL);
}
