/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Advanced Input Virtual Channel Extension
 *
 * Copyright 2022 Armin Novak <anovak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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
#include <winpr/sysinfo.h>

#include "ainput_main.h"
#include <freerdp/channels/log.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/ainput.h>
#include <freerdp/channels/ainput.h>

#include "../common/ainput_common.h"

#define TAG CHANNELS_TAG("ainput.client")

typedef struct AINPUT_PLUGIN_ AINPUT_PLUGIN;
struct AINPUT_PLUGIN_
{
	GENERIC_DYNVC_PLUGIN base;
	AInputClientContext* context;
	UINT32 MajorVersion;
	UINT32 MinorVersion;
};

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ainput_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	UINT16 type;
	AINPUT_PLUGIN* ainput;
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;

	WINPR_ASSERT(callback);
	WINPR_ASSERT(data);

	ainput = (AINPUT_PLUGIN*)callback->plugin;
	WINPR_ASSERT(ainput);

	if (!Stream_CheckAndLogRequiredLength(TAG, data, 2))
		return ERROR_NO_DATA;
	Stream_Read_UINT16(data, type);
	switch (type)
	{
		case MSG_AINPUT_VERSION:
			if (!Stream_CheckAndLogRequiredLength(TAG, data, 8))
				return ERROR_NO_DATA;
			Stream_Read_UINT32(data, ainput->MajorVersion);
			Stream_Read_UINT32(data, ainput->MinorVersion);
			break;
		default:
			WLog_WARN(TAG, "Received unsupported message type 0x%04" PRIx16, type);
			break;
	}

	return CHANNEL_RC_OK;
}

static UINT ainput_send_input_event(AInputClientContext* context, UINT64 flags, INT32 x, INT32 y)
{
	AINPUT_PLUGIN* ainput;
	GENERIC_CHANNEL_CALLBACK* callback;
	BYTE buffer[32] = { 0 };
	UINT64 time;
	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticInit(&sbuffer, buffer, sizeof(buffer));

	WINPR_ASSERT(s);
	WINPR_ASSERT(context);

	time = GetTickCount64();
	ainput = (AINPUT_PLUGIN*)context->handle;
	WINPR_ASSERT(ainput);

	if (ainput->MajorVersion != AINPUT_VERSION_MAJOR)
	{
		WLog_WARN(TAG, "Unsupported channel version %" PRIu32 ".%" PRIu32 ", aborting.",
		          ainput->MajorVersion, ainput->MinorVersion);
		return CHANNEL_RC_UNSUPPORTED_VERSION;
	}
	callback = ainput->base.listener_callback->channel_callback;
	WINPR_ASSERT(callback);

	{
		char ebuffer[128] = { 0 };
		WLog_VRB(TAG, "[%s] sending timestamp=0x%08" PRIx64 ", flags=%s, %" PRId32 "x%" PRId32,
		         __FUNCTION__, time, ainput_flags_to_string(flags, ebuffer, sizeof(ebuffer)), x, y);
	}

	/* Message type */
	Stream_Write_UINT16(s, MSG_AINPUT_MOUSE);

	/* Event data */
	Stream_Write_UINT64(s, time);
	Stream_Write_UINT64(s, flags);
	Stream_Write_INT32(s, x);
	Stream_Write_INT32(s, y);
	Stream_SealLength(s);

	/* ainput back what we have received. AINPUT does not have any message IDs. */
	WINPR_ASSERT(callback->channel);
	WINPR_ASSERT(callback->channel->Write);
	return callback->channel->Write(callback->channel, (ULONG)Stream_Length(s), Stream_Buffer(s),
	                                NULL);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ainput_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;

	free(callback);

	return CHANNEL_RC_OK;
}

static UINT init_plugin_cb(GENERIC_DYNVC_PLUGIN* base, rdpContext* rcontext, rdpSettings* settings)
{
	AINPUT_PLUGIN* ainput = (AINPUT_PLUGIN*)base;
	AInputClientContext* context = (AInputClientContext*)calloc(1, sizeof(AInputClientContext));
	if (!context)
		return CHANNEL_RC_NO_MEMORY;

	context->handle = (void*)base;
	context->AInputSendInputEvent = ainput_send_input_event;

	ainput->context = context;
	ainput->base.iface.pInterface = context;
	return CHANNEL_RC_OK;
}

static void terminate_plugin_cb(GENERIC_DYNVC_PLUGIN* base)
{
	AINPUT_PLUGIN* ainput = (AINPUT_PLUGIN*)base;
	free(ainput->context);
}

static const IWTSVirtualChannelCallback ainput_functions = { ainput_on_data_received,
	                                                         NULL, /* Open */
	                                                         ainput_on_close };

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT ainput_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return freerdp_generic_DVCPluginEntry(pEntryPoints, TAG, AINPUT_DVC_CHANNEL_NAME,
	                                      sizeof(AINPUT_PLUGIN), sizeof(GENERIC_CHANNEL_CALLBACK),
	                                      &ainput_functions, init_plugin_cb, terminate_plugin_cb);
}
