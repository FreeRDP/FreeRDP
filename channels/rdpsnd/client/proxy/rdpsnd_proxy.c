/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP rdpsnd proxy subsystem
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/cmdline.h>
#include <winpr/sysinfo.h>
#include <freerdp/types.h>
#include <freerdp/server/rdpsnd.h>
#include <freerdp/client/rdpsnd.h>

#include "rdpsnd_main.h"
#include <freerdp/server/proxy/proxy_context.h>

typedef struct rdpsnd_proxy_plugin rdpsndProxyPlugin;

struct rdpsnd_proxy_plugin
{
	rdpsndDevicePlugin device;
	RdpsndServerContext* rdpsnd_server;
};

static BOOL rdpsnd_proxy_open(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
                              UINT32 latency)
{
	rdpsndProxyPlugin* proxy = (rdpsndProxyPlugin*)device;

	/* update proxy's rdpsnd server latency */
	proxy->rdpsnd_server->latency = latency;
	return TRUE;
}

static void rdpsnd_proxy_close(rdpsndDevicePlugin* device)
{
	/* do nothing */
}

static BOOL rdpsnd_proxy_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	rdpsndProxyPlugin* proxy = (rdpsndProxyPlugin*)device;
	proxy->rdpsnd_server->SetVolume(proxy->rdpsnd_server, value, value);
	return TRUE;
}

static void rdpsnd_proxy_free(rdpsndDevicePlugin* device)
{
	rdpsndProxyPlugin* proxy = (rdpsndProxyPlugin*)device;

	if (!proxy)
		return;

	free(proxy);
}

static BOOL rdpsnd_proxy_format_supported(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format)
{
	rdpsndProxyPlugin* proxy = (rdpsndProxyPlugin*)device;

	/* use the same format that proxy's server used */
	if (proxy->rdpsnd_server->selected_client_format == format->wFormatTag)
		return TRUE;

	return FALSE;
}

static UINT rdpsnd_proxy_play(rdpsndDevicePlugin* device, const BYTE* data, size_t size)
{
	rdpsndProxyPlugin* proxy = (rdpsndProxyPlugin*)device;
	UINT64 start = GetTickCount();
	proxy->rdpsnd_server->SendSamples(proxy->rdpsnd_server, data, size / 4, start);
	return GetTickCount() - start;
}


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */

#ifdef BUILTIN_CHANNELS
#define freerdp_rdpsnd_client_subsystem_entry proxy_freerdp_rdpsnd_client_subsystem_entry
#else
#define freerdp_rdpsnd_client_subsystem_entry FREERDP_API freerdp_rdpsnd_client_subsystem_entry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	const ADDIN_ARGV* args;
	rdpsndProxyPlugin* proxy;
	pClientContext* pc;
	proxy = (rdpsndProxyPlugin*)calloc(1, sizeof(rdpsndProxyPlugin));

	if (!proxy)
		return CHANNEL_RC_NO_MEMORY;

	proxy->device.Open = rdpsnd_proxy_open;
	proxy->device.FormatSupported = rdpsnd_proxy_format_supported;
	proxy->device.SetVolume = rdpsnd_proxy_set_volume;
	proxy->device.Play = rdpsnd_proxy_play;
	proxy->device.Close = rdpsnd_proxy_close;
	proxy->device.Free = rdpsnd_proxy_free;
	args = pEntryPoints->args;

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, &proxy->device);
	pc = (pClientContext*)freerdp_rdpsnd_get_context(pEntryPoints->rdpsnd);
	if (pc == NULL)
	{
		free(proxy);
		return ERROR_INTERNAL_ERROR;
	}

	proxy->rdpsnd_server = pc->pdata->ps->rdpsnd;
	return CHANNEL_RC_OK;
}
