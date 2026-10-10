/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Authentication redirection virtual channel
 *
 * Copyright 2023 David Fort <contact@hardening-consulting.com>
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
#include <winpr/assert.h>
#include <winpr/wtypes.h>
#include <winpr/stream.h>

#include <freerdp/config.h>
#include <freerdp/freerdp.h>
#include <freerdp/addin.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/log.h>
#include <freerdp/channels/rdpear.h>

#include "rdpear_inproc.h"

#define TAG CHANNELS_TAG("rdpear.client")

/* The client of non-Windows builds without the lsa4posix daemon: every redirected [MS-RDPEAR] call
 * is serviced in process (rdpear_inproc.c). */
typedef struct
{
	GENERIC_DYNVC_PLUGIN base;
	rdpearInproc* inproc;
} RDPEAR_PLUGIN;

/**
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpear_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* s)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	WINPR_ASSERT(callback);

	RDPEAR_PLUGIN* rdpear = (RDPEAR_PLUGIN*)callback->plugin;
	WINPR_ASSERT(rdpear);

	return rdpear_inproc_receive(rdpear->inproc, callback->channel, s);
}

/**
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpear_on_open(IWTSVirtualChannelCallback* pChannelCallback)
{
	WINPR_UNUSED(pChannelCallback);
	return CHANNEL_RC_OK;
}

/**
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpear_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	WINPR_UNUSED(pChannelCallback);
	return CHANNEL_RC_OK;
}

static void terminate_plugin_cb(GENERIC_DYNVC_PLUGIN* base)
{
	WINPR_ASSERT(base);

	RDPEAR_PLUGIN* rdpear = (RDPEAR_PLUGIN*)base;
	rdpear_inproc_free(rdpear->inproc);
	rdpear->inproc = nullptr;
}

static UINT init_plugin_cb(GENERIC_DYNVC_PLUGIN* base, rdpContext* rcontext, rdpSettings* settings)
{
	WINPR_ASSERT(base);
	WINPR_UNUSED(settings);

	RDPEAR_PLUGIN* rdpear = (RDPEAR_PLUGIN*)base;
	rdpear->inproc = rdpear_inproc_new(rcontext);
	if (!rdpear->inproc)
		return CHANNEL_RC_INITIALIZATION_ERROR;
	return CHANNEL_RC_OK;
}

static const IWTSVirtualChannelCallback rdpear_callbacks = { rdpear_on_data_received,
	                                                         rdpear_on_open, rdpear_on_close,
	                                                         nullptr };

/**
 * @return 0 on success, otherwise a Win32 error code
 */
FREERDP_ENTRY_POINT(UINT rdpear_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints))
{
	return freerdp_generic_DVCPluginEntry(pEntryPoints, TAG, RDPEAR_DVC_CHANNEL_NAME,
	                                      sizeof(RDPEAR_PLUGIN), sizeof(GENERIC_CHANNEL_CALLBACK),
	                                      &rdpear_callbacks, init_plugin_cb, terminate_plugin_cb);
}
